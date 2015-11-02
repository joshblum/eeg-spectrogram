#include <armadillo>

#include "wslib/server_ws.hpp"
#include "json11/json11.hpp"
#include "storage/backends.hpp"
#include "compute/helpers.hpp"
#include "compute/eeg_spectrogram.hpp"
#include "compute/eeg_change_point.hpp"

using namespace arma;
using namespace std;
using namespace json11;

#define NUM_THREADS 4
#define PORT 8080
#define TEXT_OPCODE 129
#define BINARY_OPCODE 130

typedef SimpleWeb::SocketServer<SimpleWeb::WS> WsServer;

string CH_NAME_MAP[] = {"LL", "LP", "RP", "RL"};

void send_message(WsServer* server, shared_ptr<WsServer::Connection> connection,
                  string msg_type, Json content, float* data, size_t data_size)
{
  unsigned long long start = getticks();
  Json msg = Json::object
  {
    {"type", msg_type},
    {"content", content}
  };
  string header = msg.dump();

  uint32_t header_len = header.size() + (8 - ((header.size() + 4) % 8));
  // append enough spaces so that the payload starts at an 8-byte
  // aligned position. The first four bytes will be the length of
  // the header, encoded as a 32 bit signed integer:
  header.resize(header_len, ' ');

  auto send_stream = make_shared<WsServer::SendStream>();
  send_stream->write((char*) &header_len, sizeof(uint32_t));
  send_stream->write(header.c_str(), header_len);
  if (data != NULL)
  {
    send_stream->write((char*) data, data_size);
  }

  string action = content["action"].string_value();
  // server.send is an asynchronous function
  server->send(connection, send_stream, [action, start](const boost::system::error_code & ec)
  {
    log_time_diff("send_message::" + action, start);
    if (ec)
    {
      cout << "Server: Error sending message. " <<
           // See http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference.html
           // Error Codes for error code meanings
           "Error: " << ec << ", error message: " << ec.message() << endl;
    }
  }, BINARY_OPCODE);
}

void log_json(Json content)
{
  cout << "Sending content " << content.dump() << endl;
}

void send_frowvec(WsServer* server,
                  shared_ptr<WsServer::Connection> connection,
                  string canvasId, string type,
                  frowvec vector)
{

  Json content = Json::object
  {
    {"action", "change_points"},
    {"type", type},
    {"canvasId", canvasId}
  };
  log_json(content);
  send_message(server, connection, "spectrogram", content, vector.memptr(), vector.n_elem);
}

void send_spectrogram_new(WsServer* server,
                          shared_ptr<WsServer::Connection> connection,
                          spec_params_t spec_params, string canvasId)
{
  Json content = Json::object
  {
    {"action", "new"},
    {"nblocks", spec_params.nblocks},
    {"nfreqs", spec_params.nfreqs},
    {"fs", spec_params.fs},
    {"startTime", spec_params.start_time},
    {"endTime", spec_params.end_time},
    {"canvasId", canvasId}
  };
  log_json(content);
  send_message(server, connection, "spectrogram", content, NULL, -1);
}

void send_spectrogram_update(WsServer* server,
                             shared_ptr<WsServer::Connection> connection,
                             spec_params_t spec_params, string canvasId,
                             fmat& spec_mat)
{

  Json content = Json::object
  {
    {"action", "update"},
    {"nblocks", spec_params.nblocks},
    {"nfreqs", spec_params.nfreqs},
    {"canvasId", canvasId}
  };
  size_t data_size = sizeof(float) * spec_mat.n_elem;
  float* spec_arr = (float*) malloc(data_size);
  serialize_spec_mat(&spec_params, spec_mat, spec_arr);
  log_json(content);
  send_message(server, connection, "spectrogram", content, spec_arr, data_size);
  free(spec_arr);
}

void send_change_points(WsServer* server,
                        shared_ptr<WsServer::Connection> connection,
                        string canvasId,
                        cp_data_t* cp_data)
{
  send_frowvec(server, connection, canvasId, "change_points", cp_data->cp);
  send_frowvec(server, connection, canvasId, "summed_signal", cp_data->m);
}


void on_file_spectrogram(WsServer* server, shared_ptr<WsServer::Connection> connection, Json data)
{
  // TODO(joshblum): add data validation
  string mrn = data["mrn"].string_value();
  float start_time = data["startTime"].number_value();
  float end_time = data["endTime"].number_value();
  int ch = data["channel"].number_value();
  string ch_name = CH_NAME_MAP[ch];

  EDFBackend backend; // perhaps this should be a global thing..
  spec_params_t spec_params;
  get_eeg_spectrogram_params(&spec_params, &backend, mrn, start_time, end_time);
  print_spec_params_t(&spec_params);
  cout << endl; // print newline between each spectrogram computation

  send_spectrogram_new(server, connection, spec_params, ch_name);

  fmat spec_mat = fmat(spec_params.nfreqs, spec_params.nblocks);
  unsigned long long start = getticks();
  eeg_spectrogram(&spec_params, ch, spec_mat);
  log_time_diff("eeg_spectrogram", start);

  send_spectrogram_update(server, connection, spec_params, ch_name, spec_mat);

  cp_data_t cp_data;
  get_change_points(spec_mat, &cp_data);
  send_change_points(server, connection, ch_name, &cp_data);

  backend.close_array(mrn);
}

void receive_message(WsServer* server, shared_ptr<WsServer::Connection> connection, string type, Json content)
{
  if (type == "request_file_spectrogram")
  {
    on_file_spectrogram(server, connection, content);
  }
  else if (type == "information")
  {
    cout << content.string_value() << endl;
  }
  else
  {
    cout << "Unknown type: " << type << " and content: " << content.string_value() << endl;
  }
}

int main(int argc, char* argv[])
{
  int port;
  if (argc == 2)
  {
    port = atoi(argv[1]);
  }
  else
  {
    port = PORT;
  }

  // WebSocket (WS)-server at port using NUM_THREADS threads
  WsServer server(port, NUM_THREADS);

  auto& ws = server.endpoint["^/compute/spectrogram/?$"];

  // C++14, lambda parameters declared with auto
  // For C++11 use: (shared_ptr<WsServer::Connection> connection, shared_ptr<WsServer::Message> message)
  ws.onmessage = [&server](shared_ptr<WsServer::Connection> connection, shared_ptr<WsServer::Message> message)
  {
    auto message_str = message->string();
    string err;
    Json json = Json::parse(message_str, err);

    // TODO add error checking for null fields
    string type = json["type"].string_value();
    Json content = json["content"];

    receive_message(&server, connection, type, content);
  };

  ws.onopen = [](shared_ptr<WsServer::Connection> connection)
  {
    cout << "WebSocket opened" << endl;
  };


  // See RFC 6455 7.4.1. for status codes
  ws.onclose = [](shared_ptr<WsServer::Connection> connection, int status, const string & reason)
  {
    cout << "Server: Closed connection " << (size_t)connection.get() << " with status code " << status << endl;
  };

  // See http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference.html, Error Codes for error code meanings
  ws.onerror = [](shared_ptr<WsServer::Connection> connection, const boost::system::error_code & ec)
  {
    cout << "Server: Error in connection " << (size_t)connection.get() << ". " <<
         "Error: " << ec << ", error message: " << ec.message() << endl;
  };


  thread server_thread([&server, port]()
  {
    cout << "WebSocket Server started at port: " << port << endl;
    // Start WS-server
    server.start();
  });

  server_thread.join();

  return 0;
}

