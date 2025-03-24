//Portions of code from zNoctum and redline2466

let toUtf8Decoder = new TextDecoder( "utf-8" );
function toUTF8(ptr) {
	let len = 0|0; ptr |= 0;
	for( let i = ptr; HEAPU8[i] != 0; i++) len++;
	return toUtf8Decoder.decode(HEAPU8.subarray(ptr, ptr+len));
}

let wasmExports;
const DATA_ADDR = 16|0; // Where the unwind/rewind data structure will live.
let rendering = false;
let fullscreen = false;

//Configure WebGL Stuff (allow to be part of global context)
let canvas = document.getElementById('canvas');
let wgl = canvas.getContext('webgl');
if( !wgl )
{
	//Janky - on Firefox 83, with NVIDIA GPU, you need to ask twice.
	wgl = canvas.getContext('webgl');
}
let wglShader = null; //Standard flat color shader
let wglABV = null;    //Array buffer for vertices
let wglABC = null;    //Array buffer for colors.
let wglUXFRM = null;  //Uniform location for transform on solid colors


//Utility stuff for WebGL sahder creation.
function wgl_makeShader( vertText, fragText )
{
	let vert = wgl.createShader(wgl.VERTEX_SHADER);
	wgl.shaderSource(vert, vertText );
	wgl.compileShader(vert);
	if (!wgl.getShaderParameter(vert, wgl.COMPILE_STATUS)) {
			alert(wgl.getShaderInfoLog(vert));
	}

	let frag = wgl.createShader(wgl.FRAGMENT_SHADER);
	wgl.shaderSource(frag, fragText );
	wgl.compileShader(frag);
	if (!wgl.getShaderParameter(frag, wgl.COMPILE_STATUS)) {
			alert(wgl.getShaderInfoLog(frag));
	}
	let ret = wgl.createProgram();
	wgl.attachShader(ret, frag);
	wgl.attachShader(ret, vert);
	wgl.linkProgram(ret);
	wgl.bindAttribLocation( ret, 0, "a0" );
	wgl.bindAttribLocation( ret, 1, "a1" );
	return ret;
}

{
	//We load two shaders, one is a solid-color shader, for most rawdraw objects.
	wglShader = wgl_makeShader( 
		"uniform vec4 xfrm; attribute vec3 a0; attribute vec4 a1; varying vec4 vc; void main() { gl_Position = vec4( a0.xy*xfrm.xy+xfrm.zw, a0.z, 0.5 ); vc = a1; }",
		"precision mediump float; varying vec4 vc; void main() { gl_FragColor = vec4(vc.xyzw); }" );

	wglUXFRM = wgl.getUniformLocation(wglShader, "xfrm" );

	//Compile the shaders.
	wgl.useProgram(wglShader);

	//Get some vertex/color buffers, to put geometry in.
	wglABV = wgl.createBuffer();
	wglABC = wgl.createBuffer();

	//We're using two buffers, so just enable them, now.
	wgl.enableVertexAttribArray(0);
	wgl.enableVertexAttribArray(1);

	//Enable alpha blending
	wgl.enable(wgl.BLEND);
	wgl.blendFunc(wgl.SRC_ALPHA, wgl.ONE_MINUS_SRC_ALPHA);
}

//Do webgl work that must happen every frame.
function FrameStart()
{
	//Fixup canvas sizes
	if( fullscreen )
	{
		wgl.viewportWidth = canvas.width = window.innerWidth;
		wgl.viewportHeight = canvas.height = window.innerHeight;
	}
	
	//Make sure viewport and input to shader is correct.
	//We do this so we can pass literal coordinates into the shader.
	wgl.viewport( 0, 0, wgl.viewportWidth, wgl.viewportHeight );

	//Update geometry transform (Scale/shift)
	wgl.uniform4f( wglUXFRM, 
		1./wgl.viewportWidth, -1./wgl.viewportHeight,
		-0.5, 0.5);
}

function SystemStart( title, w, h )
{
	document.title = toUTF8( title );
	wgl.viewportWidth = canvas.width = w;
	wgl.viewportHeight = canvas.height = h;
}

//Buffered geometry system.
//This handles buffering a bunch of lines/segments, and using them all at once.
globalv = null;

function CNFGEmitBackendTrianglesJS( vertsF, colorsI, vertcount )
{
	const ab = wgl.ARRAY_BUFFER;
	wgl.bindBuffer(ab, wglABV);
	wgl.bufferData(ab, vertsF, wgl.DYNAMIC_DRAW);
	wgl.vertexAttribPointer(0, 3, wgl.FLOAT, false, 0, 0);
	wgl.bindBuffer(ab, wglABC);
	wgl.bufferData(ab, colorsI, wgl.DYNAMIC_DRAW);
	wgl.vertexAttribPointer(1, 4, wgl.UNSIGNED_BYTE, true, 0, 0);
	wgl.drawArrays(wgl.TRIANGLES, 0, vertcount );
	globalv = vertsF;
}

//This defines the list of imports, the things that C will be importing from Javascript.
//To use functions here, just call them.  Surprisingly, signatures justwork.
let imports = {
	env: {
		//Various draw-functions.
		CNFGEmitBackendTriangles : (vertsF, colorsI, vertcount )=>
		{
			//Take a float* and uint32_t* of vertices, and flat-render them.
			CNFGEmitBackendTrianglesJS(
				HEAPF32.slice(vertsF>>2,(vertsF>>2)+vertcount*3),
				HEAPU8.slice(colorsI,(colorsI)+vertcount*4),
				vertcount );
		},
		CNFGSetup : (title,w,h ) => {
			SystemStart( title, w, h );
			fullscreen = false;
		},
		CNFGSetupFullscreen : (title,w,h ) => {
			SystemStart( title, w, h );
			canvas.style = "position:absolute; top:0; left:0;"
			fullscreen = true;
		},
		CNFGClearFrameInternal: ( color ) => {
			wgl.clearColor( (color&0xff)/255., ((color>>8)&0xff)/255.,
				((color>>16)&0xff)/255., ((color>>24)&0xff)/255. ); 
			wgl.clear( wgl.COLOR_BUFFER_BIT | wgl.COLOR_DEPTH_BIT );
		},
		CNFGGetDimensions: (pw, ph) => {
			HEAP16[pw>>1] = canvas.width;
			HEAP16[ph>>1] = canvas.height;
		},
		OGGetAbsoluteTime : () => { return performance.now() * 1000;  },

		Add1 : (i) => { return i+1; }, //Super simple function for speed testing.

		//Tricky - math functions just automatically link through.
		sin : Math.sin, 
		cos : Math.cos,
		tan : Math.tan,
		sinf : Math.sin,
		cosf : Math.cos,
		tanf : Math.tan,

		saveHighScore : (hs) => { localStorage.setItem("highScore", hs); },
		getHighScore : () => { return localStorage.getItem("highScore"); },

		//Quick-and-dirty debug.
		print: console.log,
		prints: (str) => { console.log(toUTF8(str)); },
	}
};

if( !RAWDRAW_USE_LOOP_FUNCTION )
{
	imports.bynsyncify = {
		//Any javascript functions which may unwind the stack should be placed here.
		CNFGSwapBuffersInternal: () => {
			if (!rendering) {
				// We are called in order to start a sleep/unwind.
				// Fill in the data structure. The first value has the stack location,
				// which for simplicity we can start right after the data structure itself.
				HEAPU32[DATA_ADDR >> 2] = DATA_ADDR + 8;
				// The end of the stack will not be reached here anyhow.
				HEAPU32[DATA_ADDR + 4 >> 2] = 2048|0;
				wasmExports.asyncify_start_unwind(DATA_ADDR);
				rendering = true;
				// Resume after the proper delay.
				requestAnimationFrame(function() {
					FrameStart();
					wasmExports.asyncify_start_rewind(DATA_ADDR);
					// The code is now ready to rewind; to start the process, enter the
					// first function that should be on the call stack.
					wasmExports.main();
				});
			} else {
				// We are called as part of a resume/rewind. Stop sleeping.
				wasmExports.asyncify_stop_rewind();
				rendering = false;
			}
		}
	}
}

if( RAWDRAW_NEED_BLITTER )
{
	let wglBlit = null;   //Blitting shader for texture
	let wglTex = null;    //Texture handle for blitting.
	let wglUXFRMBlit = null; //Uniform location for transform on blitter

	//We are not currently supporting the software renderer.
	//We load two shaders, the other is a texture shader, for blitting things.
	wglBlit = wgl_makeShader( 
		"uniform vec4 xfrm; attribute vec3 a0; attribute vec4 a1; varying vec2 tc; void main() { gl_Position = vec4( a0.xy*xfrm.xy+xfrm.zw, a0.z, 0.5 ); tc = a1.xy; }",
		"precision mediump float; varying vec2 tc; uniform sampler2D tex; void main() { gl_FragColor = texture2D(tex,tc).wzyx;}" );

	wglUXFRMBlit = wgl.getUniformLocation(wglBlit, "xfrm" );

	imports.env.CNFGBlitImageInternal = (memptr, x, y, w, h ) => {
			if( w <= 0 || h <= 0 ) return;

			wgl.useProgram(wglBlit);

			//Most of the time we don't use textures, so don't initiate at start.
			if( wglTex == null )	wglTex = wgl.createTexture(); 

			wgl.activeTexture(wgl.TEXTURE0);
			const t2d = wgl.TEXTURE_2D;
			wgl.bindTexture(t2d, wglTex);

			//Note that unlike the normal color operation, we don't have an extra offset.
			wgl.uniform4f( wglUXFRMBlit,
				1./wgl.viewportWidth, -1./wgl.viewportHeight,
				-.5+x/wgl.viewportWidth, .5-y/wgl.viewportHeight );

			//These parameters are required.  Not sure why the defaults don't work.
			wgl.texParameteri(t2d, wgl.TEXTURE_WRAP_T, wgl.CLAMP_TO_EDGE);
			wgl.texParameteri(t2d, wgl.TEXTURE_WRAP_S, wgl.CLAMP_TO_EDGE);
			wgl.texParameteri(t2d, wgl.TEXTURE_MIN_FILTER, wgl.NEAREST);

			wgl.texImage2D(t2d, 0, wgl.RGBA, w, h, 0, wgl.RGBA,
				wgl.UNSIGNED_BYTE, new Uint8Array(memory.buffer,memptr,w*h*4) );

			CNFGEmitBackendTrianglesJS( 
				new Float32Array( [0,0,0, w,0,0,     w,h,0,       0,0,0,   w,h,0,       0,h,0 ] ),
				new Uint8Array( [0,0,0,0, 255,0,0,0, 255,255,0,0, 0,0,0,0, 255,255,0,0, 0,255,0,0] ),
				6 );

			wgl.useProgram(wglShader);
		};
}


startup = async () => {
	// Actually load the WASM blob.
	let str = atob('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');
	const byteNumbers = new Array(str.length);
	for (let i = 0; i < str.length; i++)
		byteNumbers[i] = str.charCodeAt(i);
	let blob = new Blob([new Uint8Array(byteNumbers)]);
	const ds = new DecompressionStream("deflate-raw");
	const st = blob.stream().pipeThrough(ds);
	response = await new Response(st);
	blob2 = await response.blob();
	let array = new Uint8Array(await blob2.arrayBuffer());


	WebAssembly.instantiate(array, imports).then(
		(wa) => { 
			instance = wa.instance;
			wasmExports = instance.exports;
			memory = instance.exports.memory;
			HEAPU8 = new Uint8Array(memory.buffer);
			HEAP16 = new Int16Array(memory.buffer);
			HEAPU32 = new Uint32Array(memory.buffer);
			HEAPF32 = new Float32Array(memory.buffer);


			//Attach inputs.
			if( instance.exports.HandleMotion )
			{
				canvas.addEventListener('mousemove', e => { instance.exports.HandleMotion( e.offsetX, e.offsetY, e.buttons ); } );
				canvas.addEventListener('touchmove', e => { instance.exports.HandleMotion( e.touches[0].clientX, e.touches[0].clientY, 1 ); } );
			}

			console.log( instance.exports.HandleButton );
			if( instance.exports.HandleButton )
			{
				canvas.addEventListener('mouseup', e => { instance.exports.HandleButton( e.offsetX, e.offsetY, e.button, 0 ); return false; } );
				canvas.addEventListener('mousedown', e => { console.log("XX"); instance.exports.HandleButton( e.offsetX, e.offsetY, e.button, 1 ); return false; } );
			}

			if( instance.exports.HandleKey )
			{
				document.addEventListener('keydown', e => { instance.exports.HandleKey( e.keyCode, 1 ); } );
				document.addEventListener('keyup', e => { instance.exports.HandleKey( e.keyCode, 0 ); } );
			}


			//Actually invoke main().  Note that, upon "CNFGSwapBuffers" this will 'exit'
			//But, will get re-entered from the swapbuffers animation callback.
			instance.exports.main();
			
			if( RAWDRAW_USE_LOOP_FUNCTION )
			{
				function floop() {
					FrameStart();
					requestAnimationFrame(floop);
					// The code is now ready to rewind; to start the process, enter the
					// first function that should be on the call stack.
					wasmExports.loop();
				}
				floop();
			}
		 } );

	//Code here would continue executing, but this code is executed *before* main.

}

startup()