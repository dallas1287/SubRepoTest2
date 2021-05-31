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
        const frame = instance.RetrieveFrame();
        console.log(frame.buffer.size());
        postMessage(frame);
    }
    else if(cmd === 'pause')
    {
        instance.Pause();
    }
    else if(cmd === 'next frame')
    {
        const frame = instance.RetrieveFrame();
        console.log('in worker');
        console.log(frame);
        console.log(frame.buffer.size());
        postMessage(frame);
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