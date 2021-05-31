
    // Create a worker for running Wasm code without blocking main thread.
    const worker = new Worker('worker.js');

    const input = document.querySelector('input');
    input.addEventListener('change', onFileChange);

    document.getElementById('play').addEventListener('click', onPlayClicked);
    document.getElementById('pause').addEventListener('click', onPauseClicked);
    document.getElementById('nextframe').addEventListener('click', onNextClicked); 
    document.getElementById('prevframe').addEventListener('click', onPrevClicked);

    // Listen for messages back from worker and render to DOM.
    worker.onmessage = (e) => {
        const data = e.data;
        if(!data)
        {
            console.log("no data available");
            return;
        }

        console.log(data);
        const results = document.getElementById('results');
        var ul = document.createElement('ul');  
        for(var prop in data)
        {
            console.log(`${prop}: ${data[prop]}`);
            var li = document.createElement('li');
            if(prop !== 'buffer')
            {
                li.textContent = prop + ": " + data[prop];
                ul.appendChild(li);
            }    
            //else
            //    li.textContent = prop + ": " + data.buffer.size();

        }    
        results.appendChild(ul);
    }

    // Send file to worker.
    function onFileChange(event) {
        const file = input.files[0];
        worker.postMessage(["open file", file ]);
    }

    function onPlayClicked(event) {
        worker.postMessage(["play"]);
    }
    function onPauseClicked(event) {
        worker.postMessage(['pause']);
    }
    function onNextClicked(event) {
        worker.postMessage(['next frame']);
    }
    function onPrevClicked(event) {
        worker.postMessage(['prev frame']);
    }