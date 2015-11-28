"use strict";

// http://stackoverflow.com/questions/105034/create-guid-uuid-in-javascript
function guid() {
  function s4() {
    return Math.floor((1 + Math.random()) * 0x10000)
      .toString(16)
      .substring(1);
  }
  return s4() + s4() + "-" + s4() + "-" + s4() + "-" +
    s4() + "-" + s4() + s4() + s4();
}

var VisgothLabels = {
  "bandwidth": "bandwidth",
  "networkLatency": "networkLatency",
  "fps": "fps",
  "bufferLoadTime": "bufferLoadTime",
  "networkBufferSize": "networkBufferSize",
  "extent": "extent",
};

// ************** //
//  Visgoth Stat  //
// ************** //
function VisgothStat(label, id) {
  this.label = label;
  this.id = id;
  this.start = 0;
  this.end = 0;
  this.state = [];
}

VisgothStat.prototype.addValue = function(value) {
  this.state.push(value);
};

VisgothStat.prototype.markStart = function() {
  this.start = performance.now();
};

VisgothStat.prototype.markEnd = function() {
  this.end = performance.now();
};

VisgothStat.prototype.measurePerformance = function(shouldEnd) {
  if (shouldEnd !== false) {
    this.markEnd();
  }
  this.addValue(this.end - this.start);
};

// ***************** //
//  Visgoth Profiler //
// ***************** //
function VisgothProfiler(label, getProfileDataFn) {
  this.label = label;
  this.getProfileData = getProfileDataFn;
}

VisgothProfiler.prototype.sumArray = function(array) {
  return array.reduce(function(x, y) {
    return x + y;
  }, 0);
};

VisgothProfiler.prototype.avgArray = function(array) {
  var length = array.length;
  if (length === 0) {
    return 0;
  } else {
    return this.sumArray(array) / length;
  }
};

VisgothProfiler.prototype.sumStatArray = function(statArray) {
  return this.sumArray(statArray.reduce(function(x, y) {
    return x.concat(y.state);
  }, []));
};

VisgothProfiler.prototype.avgStatArray = function(statArray) {
  return this.avgArray(statArray.reduce(function(x, y) {
    return x.concat(y.state);
  }, []));
};

var NetworkLatencyProfiler = new VisgothProfiler(VisgothLabels.networkLatency, function(stats) {
  stats = stats[this.label];
  return this.avgStatArray(stats);
});

var BandwidthProfiler = new VisgothProfiler(VisgothLabels.bandwidth, function(stats) {
  var networkLatencyStats = stats[VisgothLabels.networkLatency];
  var networkBufferSizeStats = stats[VisgothLabels.networkBufferSize];
  // should this instead be the average per request?
  return this.avgStatArray(networkBufferSizeStats) / this.avgStatArray(networkLatencyStats);
});

var FpsProfiler = new VisgothProfiler(VisgothLabels.fps, function(stats) {
  stats = stats[this.label];
  // events per second
  return 1000 / this.avgStatArray(stats);
});

var MockExtentProfiler = new VisgothProfiler(VisgothLabels.extent, function(stats) {
  var extent_state = stats[this.label][0].state;
  return extent_state[extent_state.length - 1];
});


// ********* //
//  Visgoth  //
// ********* //
function Visgoth() {
  this.profilers = [
    BandwidthProfiler,
    NetworkLatencyProfiler,
    FpsProfiler,
    MockExtentProfiler,
  ];

  this.stats = {};
  var profiler;
  for (var i in this.profilers) {
    profiler = this.profilers[i];
    this.stats[profiler.label] = [];
  }

  this.clientId = guid();
}

Visgoth.prototype.registerStat = function(visgothStat) {
  if (this.stats[visgothStat.label] === undefined) {
    this.stats[visgothStat.label] = [];
  }
  this.stats[visgothStat.label].push(visgothStat);
};

Visgoth.prototype.getProfileData = function() {
  var profile = {};
  var profiler, label;
  for (var i in this.profilers) {
    profiler = this.profilers[i];
    label = profiler.label;
    profile[label] = profiler.getProfileData(this.stats);
  }
  return profile;
};

Visgoth.prototype.getMetadata = function() {
  return {
    "client_id": this.clientId,
    "timestamp": Date.now(),
  };
};

Visgoth.prototype.sendProfiledMessage = function(type, content) {
  var visgoth_content = {
    "metadata": this.getMetadata(),
    "profile": this.getProfileData(),
  };
  sendMessage(type, content, visgoth_content);
};