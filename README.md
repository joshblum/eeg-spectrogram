WebGL-Spectrogram
=================

This is a small local web app that displays a spectrogram of an audio signal in the browser using WebGL. It is known to work with Firefox and Chrome, though performance is best in Firefox. In order to run the server, you need to have tornado, numpy and scipy installed.

The spectrogram display can be zoomed and panned smoothly and has a configurable FFT length. The amplitude range can be adjusted on the fly as well. It can open any local _*.wav_ or _*.flac_ file.

*server.py* contains a small web server written in Python and Tornado that responds to messages on `ws://localhost:8888/spectrogram`. Currently, it supports two kinds of messages: One that requests a spectrogram from a file name, and another that requests a spectrogram from a file content attached to the message. It responds to these messages with a message containing a full spectrogram for the given audio file. 

*communications.js* contains the Javascript implementation of the messaging protocol. *specsize.js* contains a helper class for storing the extent of a spectro-temporal display.

*spectrogram.js* contains Javascript code that can load audio files and request spectrograms from the server, and draw those spectrograms using WebGL. It also contains a time and frequency scale and a small indicator that shows the cursor position in time/frequency coordinates.

*main.html* contains the website used to display the spectrogram.

The messaging protocol is JSON-based, easily extensible and supports transmission of textual or binary data.
