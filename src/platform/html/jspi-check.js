// The web build with Open CASCADE runs on JSPI (WebAssembly.Suspending): say so plainly in a
// browser without it, rather than failing to load.
if(typeof WebAssembly.Suspending !== 'function') {
  var note = document.getElementById('status');
  if(note) {
    note.innerHTML = 'This version of SolveSpace NG, with Open CASCADE, needs Chrome or Edge ' +
      '137 or later. <a href="../ng/">The everyday version</a> runs in every browser.';
  }
  Module.setStatus = function() {}; // the data download, already under way, would replace it
  throw new Error('JSPI is not available');
}
