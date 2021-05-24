    // Create a worker for running Wasm code without blocking main thread.
    const worker = new Worker('worker.js');

    const input = document.querySelector('input');
    input.addEventListener('change', onFileChange);

    // Listen for messages back from worker and render to DOM.
    worker.onmessage = (e) => {
        const data = e.data;
        const results = document.getElementById('results');
        const ul = document.createElement('ul');
        const li = document.createElement('li');
        li.textContent = "format: " + data.format;
        const li2 = document.createElement('li');
        li2.textContent = "duration: " + data.duration;
        const li3 = document.createElement('li');
        li3.textContent = "streams: " + data.streams;
        ul.appendChild(li);
        ul.appendChild(li2);
        ul.appendChild(li3);
        results.appendChild(ul);
    }

    // Send file to worker.
    function onFileChange(event) {
        const file = input.files[0];
        worker.postMessage([ file ]);
    }