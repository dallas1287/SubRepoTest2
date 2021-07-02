
    // Create a worker for running Wasm code without blocking main thread.
    //const worker = new Worker('worker.js');
    // var Module = {
    //     onRuntimeInitialized: function(){
    //         var instance = new Module.MediaPlayer();
    //         console.log(instance);
    //     }
    // }
    import Module from './Builder.js'
    var module;
    Module().then(instance => {
        module = instance;
        console.log(module);
        var instance = new module.MediaPlayer();
        console.log(instance);
        instance.delete();   
    });



    // const input = document.querySelector('input');
    // input.addEventListener('change', onFileChange);

    // document.getElementById('play').addEventListener('click', onPlayClicked);
    // document.getElementById('pause').addEventListener('click', onPauseClicked);
    // document.getElementById('nextframe').addEventListener('click', onNextClicked); 
    // document.getElementById('prevframe').addEventListener('click', onPrevClicked);

    // var canvas = document.getElementById('canvas');
    // const gl = canvas.getContext('webgl');

    // var vsSource = "attribute vec3 posAttr; \
    // attribute vec3 texCoordAttr; \
    // varying vec2 TexCoords; \
    // void main() \
    // { \
    //     gl_Position = vec4(posAttr, 1.0); \
    //     TexCoords = texCoordAttr.xy; \
    // }";

    // var fsSource = "precision mediump float; \
    // varying vec2 TexCoords; \
    // uniform sampler2D onscreenTexture; \
    // void main() \
    // { \
    //     gl_FragColor = texture2D(onscreenTexture, TexCoords); \
    // }";

    // var vsShader = gl.createShader(gl.VERTEX_SHADER);
    // gl.shaderSource(vsShader, vsSource);
    // gl.compileShader(vsShader);

    // var fsShader = gl.createShader(gl.FRAGMENT_SHADER);
    // gl.shaderSource(fsShader, fsSource);
    // gl.compileShader(fsShader);

    // var program = gl.createProgram();

    // gl.attachShader(program, vsShader);
    // gl.attachShader(program, fsShader);
    // gl.linkProgram(program);

    // var vbo = gl.createBuffer();
    // gl.bindBuffer(gl.ARRAY_BUFFER, vbo);

    // var ebo = gl.createBuffer();
    // gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, ebo);

    // var vertices = 
    // [
    //     1.0, 1.0, 0.0, 1.0, 1.0, 0.0,
    //     1.0, -1.0, 0.0, 1.0, 0.0, 0.0,
    //     -1.0, -1.0, 0.0, 0.0, 0.0, 0.0,
    //     -1.0, 1.0, 0.0, 0.0, 1.0, 0.0
    // ];
    // var indices = 
    // [
    //     0,1,3,
    //     1,2,3
    // ];

    // gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(vertices), gl.STATIC_DRAW);
    // gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, new Uint16Array(indices), gl.STATIC_DRAW);

    // gl.vertexAttribPointer(0, 3, gl.FLOAT, false, 24, 0);
    // gl.vertexAttribPointer(1, 3, gl.FLOAT, false, 24, 12);
    // gl.enableVertexAttribArray(0);
    // gl.enableVertexAttribArray(1);
  
    // //texture stuff
    // const level = 0;
    // const internalFormat = gl.RGBA;
    // const width = 1920;
    // const height = 1080;
    // const border = 0;
    // const srcFormat = gl.RGBA;
    // const srcType = gl.UNSIGNED_BYTE;
    // var pixelVal = [0, 0, 255, 255];
    // const pixels = new Uint8Array(width * height * 4);  // opaque blue

    // for(var i = 0; i < width * height * 4; i += 4)
    //     pixels.set(pixelVal, i);
      
    // const texture = gl.createTexture();
    // gl.activeTexture(gl.TEXTURE0);
    // gl.bindTexture(gl.TEXTURE_2D, texture);

    // gl.texImage2D(gl.TEXTURE_2D, level, internalFormat,
    //               width, height, border, srcFormat, srcType,
    //               pixels);

    // // gl.NEAREST is also allowed, instead of gl.LINEAR, as neither mipmap.
    // gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    // // Prevents s-coordinate wrapping (repeating).
    // gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    // // Prevents t-coordinate wrapping (repeating).
    // gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);

    // gl.useProgram(program);
    // gl.drawElements(gl.TRIANGLES, indices.length, gl.UNSIGNED_SHORT, 0);
  
    // // Listen for messages back from worker and render to DOM.
    // worker.onmessage = (e) => {
    //     const data = e.data;
    //     if(!data)
    //     {
    //         console.log("no data available");
    //         return;
    //     }

    //     if(data.buffer)
    //     {
    //         console.log(data.buffer);
    //         setTextureData(data.buffer, data.width, data.height);
    //     }
    //     const results = document.getElementById('results');
    //     var ul = document.createElement('ul');  
    //     for(var prop in data)
    //     {
    //         var li = document.createElement('li');
    //         if(prop === 'framePts')
    //         {
    //             li.textContent = prop + ": " + data[prop];
    //             ul.appendChild(li);
    //         }    
    //     }    
    //     results.appendChild(ul);
    // }

    // function setTextureData(data, width, height)
    // {
    //      gl.activeTexture(gl.TEXTURE0);
    //      gl.bindTexture(gl.TEXTURE_2D, texture);
    
    //     // gl.texSubImage2D(gl.TEXTURE_2D, level, 0, 0,
    //     //               width, height, srcFormat, srcType,
    //     //               data, 0);
    //     gl.texImage2D(gl.TEXTURE_2D, level, internalFormat,
    //     width, height, border, srcFormat, srcType,
    //     data);

    //     gl.linkProgram(program);
    //     gl.useProgram(program);
    //     gl.drawElements(gl.TRIANGLES, indices.length, gl.UNSIGNED_SHORT, 0);
    // }

    // // Send file to worker.
    // function onFileChange(event) {
    //     const file = input.files[0];
    //     worker.postMessage(["open file", file ]);
    // }

    // function onPlayClicked(event) {
    //     worker.postMessage(["play"]);
    // }
    // function onPauseClicked(event) {
    //     worker.postMessage(['pause']);
    // }
    // function onNextClicked(event) {
    //     worker.postMessage(['next frame']);
    // }
    // function onPrevClicked(event) {
    //     worker.postMessage(['prev frame']);
    // }