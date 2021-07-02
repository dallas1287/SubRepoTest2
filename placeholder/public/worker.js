// Run this script as a Web Worker so it doesn't block the
// browser's main thread.

// See: index.html.
onmessage = (e) => {
    if(e.data === null)
        return;
    var instance = new Module.MediaPlayer();
    var cmd = e.data[0];
    if(cmd === 'open file')
    {
        const file = e.data[1];

        // Create and mount FS work directory.
        if (!FS.analyzePath('/work').exists) {
            FS.mkdir('/work');
        }
        FS.mount(WORKERFS, { files: [file] }, '/work');
     
        // Run the Wasm function we exported.
        const info = instance.OpenFile('/work/' + file.name);
        // Post message back to main thread.
        postMessage(info);
    
        // Unmount the work directory.
        FS.unmount('/work');
    }
    else if(cmd === 'play')
    {
        const frame = instance.RetrieveFramePtr();
        console.log(frame);

        const data = new Uint8Array(HEAPU8.buffer, frame.data, frame.width * frame.height * 4);
        console.log(data);
        Module._free(frame.data);

        postMessage({width: frame.width, height: frame.height, buffer: data});
    }
    else if(cmd === 'pause')
    {
        instance.Pause();
    }
    else if(cmd === 'next frame')
    {
        const seek = instance.SeekTo(54000);
        console.log(seek);
    }
    else if(cmd === 'prev frame')
    {
        const response = instance.SeekTo(-1);
        console.log(response);
    }

    instance.delete();
}

// Import the Wasm loader generated from our Emscripten build.
self.importScripts('Builder.js');