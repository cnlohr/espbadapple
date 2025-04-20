//Portions of code from zNoctum and redline2466

var mem;
let toUtf8Decoder = new TextDecoder( "utf-8" );
function toUTF8(ptr) {
	HEAPU8 = new Uint8Array(mem.buffer);
	let len = 0|0; ptr |= 0;
	for( let i = ptr; HEAPU8[i] != 0; i++) len++;
	return toUtf8Decoder.decode(HEAPU8.subarray(ptr, ptr+len));
}

let wasmExports;
//const DATA_ADDR = 16|0; // Where the unwind/rewind data structure will live.
let rendering = false;
let fullscreen = false;

//Configure WebGL Stuff (allow to be part of global context)
let canvas = document.getElementById('canvas');
let wgl = canvas.getContext('webgl', { antialias: false, depth: false });
let wglShader = null; //Standard flat color shader
let wglABV = null;    //Array buffer for vertices
let wglABC = null;    //Array buffer for colors.
let wglUXFRM = null;  //Uniform location for transform on solid colors

/* Audio subsystem, from lolra, example using AudioContext without needing to have extra .js files. */

var playingAudioProcessor = null;
var audioContext = null;
var mute = false;

var esetup = null;
async function SetupWebAudio()
{
	var bypass = '\
	class PlayingAudioProcessor extends AudioWorkletProcessor {\
		static get parameterDescriptors() {\
		  return [\
		  	{ name: "gain", defaultValue: 0, },\
		  	{ name: "sampleAdvance", defaultValue: 1.0, },\
			]\
		};\
		constructor() {\
			super();\
			this.rbuffer = new Float32Array(8192); \
			this.rbufferhead = 0|0; \
			this.rbuffertail = 0|0; \
			this.sampleplace = 0.0; \
			this.dcoffset = 0.0; \
			this.totalsampcount = 0|0; \
			\
			this.port.onmessage = (e) => { \
				for( var i = 0|0; i < e.data.length|0; i++ ) \
				{ \
					let n = (this.rbufferhead + (1|0))%(8192|0); \
					if( n == this.rbuffertail ) \
					{ \
						this.rbuffertail = (this.rbuffertail + (1|0))%(8192|0); \
						/*console.log( "Overflow" ); */ \
					} \
					var vv = e.data[i]; \
					this.dcoffset = this.dcoffset * 0.995 + vv * 0.005; \
					this.rbuffer[this.rbufferhead] = vv - this.dcoffset; \
					this.rbufferhead = n; \
				} \
			}; \
		}\
		\
		process(inputs, outputs, parameters) {\
			/*console.log( parameters.gain[0] );*/ \
			/*console.log( this.ingestData );*/ \
			let len = outputs[0][0].length; \
			const sa = Math.fround( parameters.sampleAdvance[0] ); /*float*/ \
			var s = Math.fround( this.sampleplace );      /*float*/ \
			var tail = this.rbuffertail | 0;              /* int*/  \
			var tailnext = this.rbuffertail | 0;          /* int*/  \
			if( tail == this.rbufferhead ) { /*console.log( "Underflow " );*/ return true; }\
			var tsamp = Math.fround( this.rbuffer[tail] ); \
			var nsamp = Math.fround( this.rbuffer[tailnext] ); \
			this.totalsampcount += len|0; \
			for (let b = 0|0; b < len|0; b++) { \
				s += sa; \
				var excess = Math.floor( s ) | 0; \
				if( excess > 0 ) \
				{ \
					s -= excess; \
					tail = ( tail + (excess|0) ) % (8192|0); \
					tailnext = ( tail + (1|0) ) % (8192|0); \
					if( tail == this.rbufferhead ) { /* console.log( "Underflow" ); */ break; } \
					tsamp = Math.fround( this.rbuffer[tail] ); \
					nsamp = Math.fround( this.rbuffer[tailnext] ); \
				} \
				var valv = tsamp * (1.0-s) + nsamp * s; \
				outputs[0][0][b] = valv*parameters.gain[0]; \
			} \
			/*console.log( tail + " " + this.rbuffertail + " " + tsamp + " " + nsamp );*/ \
			this.rbuffertail = tail; \
			this.sampleplace = s; \
			return true; \
		} \
	} \
	\
	registerProcessor("playing-audio-processor", PlayingAudioProcessor);';

	// The following mechanism does not work on Chrome.
	//	const dataURI = URL.createObjectURL( new Blob([bypass], { type: 'text/javascript', } ) );

	// Extremely tricky trick to side-step local file:// CORS issues.  (This does work on Chrome)
	// https://stackoverflow.com/a/67125196/2926815
	// https://stackoverflow.com/a/72180421/2926815
	let blob = new Blob([bypass], {type: 'application/javascript'});
	let reader = new FileReader();
	await reader.readAsDataURL(blob);
	let dataURI = await new Promise((res) => {
		reader.onloadend = function () {
			res(reader.result);
		}
	});

	audioContext = new AudioContext();

	await audioContext.audioWorklet.addModule(dataURI);

	playingAudioProcessor = new AudioWorkletNode(
		audioContext,
		"playing-audio-processor"
	);
	playingAudioProcessor.connect(audioContext.destination);
	audioContext.resume();

	let gainParam = playingAudioProcessor.parameters.get( "gain" );
	gainParam.setValueAtTime( .5, audioContext.currentTime );  // Reduce volume
}

ToggleAudio = () =>
{
	// AudioContexts need human intervention.
	if( !playingAudioProcessor )
	{
		SetupWebAudio();
		mute = false;
	}
	else
	{
		mute = !mute;
	}
	//document.getElementById( "soundButton" ).value = mute?"🔇":"🔊";
	return !mute;
}

function ChangeFavicon( pixelData, width, height )
{
	HEAPU8 = new Uint8Array(mem.buffer);

	document.head = document.head || document.getElementsByTagName('head')[0];
	var link = document.createElement('link'),
		oldLink = document.getElementById('dynamic-favicon');
	link.id = 'dynamic-favicon';
	link.rel = 'shortcut icon';

	const trailerLength = 384;

	const header = new Uint8Array( [
		0x00, 0x00, 0x01, 0x00, 0x01, 0x00,
		width, // Width
		height, // Height
		0x10, 0x00, 0x01, 0x00, 0x04, 0x00,
		0xE8, 0x07, 0x00, 0x00, // Bitmap size
		0x16, 0x00, 0x00, 0x00, // Bitmap contents
		// 3-color DIB header (in 16-color mode)
		0x28, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x01, 0x00, 0x04, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8C, 0x8C, 0x8C, 0x00,
		0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	]);

    let fullBitmap = new Uint8Array( header.byteLength + width * height / 2 + trailerLength );
	fullBitmap.set( header, 0 );
	fullBitmap.set( new Uint8Array( mem.buffer.slice(pixelData,pixelData+(width*height/2)|0) ), header.byteLength );

	var binary = '';
    var len = fullBitmap.byteLength;
    for (var i = 0; i < len; i++) {
        binary += String.fromCharCode( fullBitmap[ i ] );
    }
	const b64icon = btoa( binary );
	link.href = "data:image/x-icon;base64," + b64icon;

	if (oldLink) {
		document.head.removeChild(oldLink);
	}
	document.head.appendChild(link);
}

//ChangeFavicon();

function FeedWebAudio( audioFloat, audioSamples )
{
	if( !mute && audioContext != null && playingAudioProcessor != null )
	{
		// If we need to do a poor resample.  Some browsers seem to prefer 44100
		let sampleAdvance = (48000.0) / audioContext.sampleRate;
		//console.log( "Target Rate : " + audioContext.sampleRate );
		let sampleAdvanceParam = playingAudioProcessor.parameters.get("sampleAdvance");
		sampleAdvanceParam.setValueAtTime( 1.0, audioContext.currentTime);
		playingAudioProcessor.port.postMessage( HEAPF32.slice(audioFloat>>2,(audioFloat>>2)+audioSamples), );
	}
}



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
		"precision mediump float; varying vec4 vc; void main() { gl_FragColor = vec4(vc.wzyx); }" );

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
	document.getElementById("loading").innerHTML = "";
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
			HEAPU8 = new Uint8Array(mem.buffer);
			HEAPF32 = new Float32Array(mem.buffer);
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
		CNFGSetupFullscreen : (title, sno) => {
			w = canvas.width; h = canvas.height;
			SystemStart( title, w, h );
			canvas.style += ";position:absolute; top:0; left:0;"
			fullscreen = true;
		},
		CNFGClearFrameInternal: ( color ) => {
			wgl.clearColor( (color&0xff)/255., ((color>>8)&0xff)/255.,
				((color>>16)&0xff)/255., ((color>>24)&0xff)/255. ); 
			wgl.clear( wgl.COLOR_BUFFER_BIT | wgl.COLOR_DEPTH_BIT );
		},
		CNFGGetDimensions: (pw, ph) => {
			HEAP16 = new Int16Array(mem.buffer);
			HEAP16[pw>>1] = canvas.width;
			HEAP16[ph>>1] = canvas.height;
		},
		OGGetAbsoluteTime : () => { return performance.now() / 1000.0;  },

		Add1 : (i) => { return i+1; }, //Super simple function for speed testing.

		//Tricky - math functions just automatically link through.
		sin   : Math.sin, 
		cos   : Math.cos,
		tan   : Math.tan,
		atan2 : Math.atan2,
		sinf  : Math.sin,
		cosf  : Math.cos,
		tanf  : Math.tan,
		exp   : Math.exp,
		log   : Math.log,

		FeedWebAudio : FeedWebAudio,
		ChangeFavicon : ChangeFavicon,
		ToggleAudio : ToggleAudio,
		ToggleFullscreen : () => {
			const ce = document.getElementById('canvas');
			var isFullscreen = document.fullscreenElement == ce;
			if(isFullscreen)
				document.exitFullscreen();
			else
				ce.requestFullscreen();
		},
		NavigateLink : (str) => { document.location.href = toUTF8(str); },
		ChangeCursorToPointer = ( yes ) => { document.getElementById('canvas').style.cursor = yes?"pointer":"default" },
		IsFullscreen : () => { return document.fullscreenElement == document.getElementById('canvas'); },

		CNFGSetScissorsInternal : ( xywh ) => {
			wgl.enable( wgl.SCISSOR_TEST );
			HEAPU32 = new Uint32Array(mem.buffer);
			wgl.scissor( HEAPU32[(xywh>>2)+0], HEAPU32[(xywh>>2)+1], HEAPU32[(xywh>>2)+2], HEAPU32[(xywh>>2)+3] );
		},

		CNFGGetScissorsInternal : ( xywh ) => {
			HEAPU32 = new Uint32Array(mem.buffer);
			var xywha = wgl.getParameter( wgl.SCISSOR_BOX );
			HEAPU32[(xywh>>2)+0] = xywha[0];
			HEAPU32[(xywh>>2)+1] = xywha[1];
			HEAPU32[(xywh>>2)+2] = xywha[2];
			HEAPU32[(xywh>>2)+3] = xywha[3];
		},

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
				HEAPU32 = new Uint32Array(mem.buffer);
				// We are called in order to start a sleep/unwind.
				// Fill in the data structure. The first value has the stack location,
				// which for simplicity we can start right after the data structure itself.
				HEAPU32[instance.exports.asyncify_struct >> 2] = instance.exports.asyncify_struct+8;
				// The end of the stack will not be reached here anyhow.
				HEAPU32[(instance.exports.asyncify_struct + 4) >> 2] = instance.exports.asyncify_struct+8192|0;
				wasmExports.asyncify_start_unwind(instance.exports.asyncify_struct);
				rendering = true;
				// Resume after the proper delay.
				requestAnimationFrame(function() {
					FrameStart();
					wasmExports.asyncify_start_rewind(instance.exports.asyncify_struct);
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
				wgl.UNSIGNED_BYTE, new Uint8Array(mem.buffer,memptr,w*h*4) );

			CNFGEmitBackendTrianglesJS( 
				new Float32Array( [0,0,0, w,0,0,     w,h,0,       0,0,0,   w,h,0,       0,h,0 ] ),
				new Uint8Array( [0,0,0,0, 255,0,0,0, 255,255,0,0, 0,0,0,0, 255,255,0,0, 0,255,0,0] ),
				6 );

			wgl.useProgram(wglShader);
		};
}

startup = async () => {
	// Actually load the WASM blob.
	let str = atob('${BLOB}');
	const byteNumbers = new Array(str.length);
	for (let i = 0; i < str.length; i++)
		byteNumbers[i] = str.charCodeAt(i);
	let blob = new Blob([new Uint8Array(byteNumbers)]);
	const ds = new DecompressionStream("deflate-raw");
	const st = blob.stream().pipeThrough(ds);
	response = await new Response(st);
	blob2 = await response.blob();
	let array = new Uint8Array(await blob2.arrayBuffer());

	mem = imports.env.memory = new WebAssembly.Memory({
			initial: 16384,
			maximum: 16384,
			shared: false,
		});

	WebAssembly.instantiate(array, imports).then(
		(wa) => { 
			instance = wa.instance;
			wasmExports = instance.exports;

			//Attach inputs.
			if( instance.exports.HandleMotion )
			{
				canvas.addEventListener('mousemove', e => { instance.exports.HandleMotion( e.offsetX, e.offsetY, e.buttons ); } );
				canvas.addEventListener('touchmove', e => { instance.exports.HandleMotion( e.touches.length?e.touches[0].clientX:0, e.touches.length?e.touches[0].clientY:0, e.touches.length?1:0 ); } );
			}

			if( instance.exports.HandleButton )
			{
				var touchEvent = ( e, d ) => {
					instance.exports.HandleButton( e.offsetX, e.offsetY, e.button, d );
					//instance.exports.HandleMotion( e.offsetX, e.offsetY, e.buttons );
					//e.preventDefault();
					return false;
				};
				canvas.addEventListener('mouseup',    e => { return touchEvent( e, 0 ); }, false );
				canvas.addEventListener('mousedown',  e => { return touchEvent( e, 1 ); }, false );
				canvas.addEventListener('touchend',   e => { instance.exports.HandleButton( e.touches.length?e.touches[0].clientX:0, e.touches.length?e.touches[0].clientY:0, 0, 0 ); return false; }, false );
				canvas.addEventListener('touchstart', e => { instance.exports.HandleButton( e.touches.length?e.touches[0].clientX:0, e.touches.length?e.touches[0].clientY:0, 0, e.touches.length?1:0 ); return false; }, false );
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

startup();

