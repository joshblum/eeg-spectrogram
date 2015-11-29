#include "backends.hpp"

#include<string>
#include<armadillo>

using namespace std;
using namespace arma;

#define DATA_RANK 2
#define ATTR_RANK 1
#define NUM_ATTR 4
#define ATTR_NAME "metadata" // we store fs and nsamples here, all datasets have the same attribute name
#define FS_IDX 0
#define NSAMPLES_IDX 1
#define NROWS_IDX 2
#define NCOLS_IDX 3

string HDF5Backend::mrn_to_array_name(string mrn)
{
  return _mrn_to_array_name(mrn, ".h5");
}

ArrayMetadata HDF5Backend::get_array_metadata(string mrn)
{
  DataSet dataset = get_cache(mrn);
  Attribute attr = dataset.openAttribute(ATTR_NAME);
  int attr_data[NUM_ATTR];
  attr.read(PredType::NATIVE_INT, attr_data);
  int fs = attr_data[FS_IDX];
  int nsamples = attr_data[NSAMPLES_IDX];
  int nrows = attr_data[NROWS_IDX];
  int ncols = attr_data[NCOLS_IDX];
  return ArrayMetadata(fs, nsamples, nrows, ncols);
}

void HDF5Backend::create_array(string mrn, ArrayMetadata* metadata)
{
  string array_name = mrn_to_array_name(mrn);
  H5File file(array_name, H5F_ACC_TRUNC);

  // Create the dataset of the correct dimensions
  hsize_t data_dims[DATA_RANK];
  data_dims[0] = metadata->nrows;
  data_dims[1] = metadata->ncols;
  DataSpace dataspace(DATA_RANK, data_dims);
  DataSet dataset = file.createDataSet(mrn, PredType::NATIVE_FLOAT, dataspace);

  int attr_data[NUM_ATTR];
  attr_data[FS_IDX] = metadata->fs;
  attr_data[NSAMPLES_IDX] = metadata->nsamples;
  attr_data[NROWS_IDX] = metadata->nrows;
  attr_data[NCOLS_IDX] = metadata->ncols;
  hsize_t attr_dims[ATTR_RANK] = {NUM_ATTR};
  DataSpace attrspace = DataSpace(ATTR_RANK, attr_dims);

  Attribute attribute = dataset.createAttribute(ATTR_NAME, PredType::STD_I32BE, attrspace);

  attribute.write(PredType::NATIVE_INT, attr_data);
}

void HDF5Backend::open_array(string mrn)
{
  if (!array_exists(mrn)) {
    cout << "Error array " << mrn_to_array_name(mrn) << " does not exist!" << endl;
    exit(1);
  }
  if (in_cache(mrn))
  {
    return;
  }
  string array_name = mrn_to_array_name(mrn);
  H5File file(array_name, H5F_ACC_RDWR);
  DataSet dataset = file.openDataSet(mrn);
  put_cache(mrn, dataset);
}

void HDF5Backend::read_array(string mrn, int ch, int start_offset, int end_offset, frowvec& buf)
{
  DataSet dataset = get_cache(mrn);
  hsize_t offset[DATA_RANK], count[DATA_RANK];

  offset[0] = start_offset; // start_offset rows down
  offset[1] = CH_REVERSE_IDX[ch]; // get the correct column

  count[0] = end_offset - start_offset;
  count[1] = buf.n_rows; // only ever get one column

  _read_array(mrn, offset, count, buf.memptr());
}

void HDF5Backend::read_array(string mrn, int start_offset, int end_offset, fmat& buf)
{
  hsize_t offset[DATA_RANK], count[DATA_RANK];
  offset[0] = start_offset; // start_offset rows down
  offset[1] = 0; //  we want all columns

  count[0] = end_offset - start_offset;
  count[1] = buf.n_rows;
  _read_array(mrn, offset, count, buf.memptr());
}

void HDF5Backend::_read_array(string mrn, hsize_t offset[], hsize_t count[], void* buf)
{
  DataSet dataset = get_cache(mrn);
  hsize_t stride[DATA_RANK], block[DATA_RANK];

  // TODO(joshblum): use this for downsampling
  stride[0] = 1;
  stride[1] = 1;

  block[0] = 1;
  block[1] = 1;

  DataSpace memspace(DATA_RANK, count, NULL);
  DataSpace dataspace = dataset.getSpace();
  dataspace.selectHyperslab(H5S_SELECT_SET, count, offset, stride, block);

  dataset.read(buf, PredType::NATIVE_FLOAT, memspace, dataspace);
}

void HDF5Backend::write_array(string mrn, int ch, int start_offset, int end_offset, fmat& buf)
{
  DataSet dataset = get_cache(mrn);
  hsize_t offset[DATA_RANK], count[DATA_RANK];

  offset[0] = start_offset; // start_offset rows down
  offset[1] = CH_REVERSE_IDX[ch]; // get the correct column

  count[0] = end_offset - start_offset;
  count[1] = buf.n_rows;

  DataSpace dataspace = dataset.getSpace();
  dataspace.selectHyperslab(H5S_SELECT_SET, count, offset);

  DataSpace memspace(DATA_RANK, count, NULL);
  dataset.write(buf.memptr(), PredType::NATIVE_FLOAT, memspace, dataspace);

}

void HDF5Backend::close_array(string mrn)
{
  if (in_cache(mrn)) {
    DataSet dataset = get_cache(mrn);
    dataset.close();
    pop_cache(mrn);
  }
}

