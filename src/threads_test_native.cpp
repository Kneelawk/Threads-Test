#include <nan.h>
#include <thread>
#include <atomic>
#include <iostream>

// fractal generation settings
int width;
int height;
float fractalWidth;
float fractalHeight;
float fractalX;
float fractalY;
int iterations;

// fractal generation thread
std::thread *t = NULL;

// fractal generation stats
std::atomic_bool generating(false);
std::atomic_uint progress(0);

// fractal pixel buffer
v8::Global<v8::Object> buffer;
char *data = NULL;

// libuv async thread merging callback handle
uv_async_t *doneAsync;

// javascript callback handle
v8::Global<v8::Function> doneCallbackFunc;

typedef struct {
	char r;
	char g;
	char b;
} rgb_data;

// calculate rgb data from hsb data
// this was copied from the java library's java.awt.Color
rgb_data fromHSB(float hue, float saturation, float brightness) {
	int r = 0, g = 0, b = 0;
	if (saturation == 0) {
		r = g = b = (int) (brightness * 255.0 + 0.5);
	} else {
		float h = (hue - floor(hue)) * 6.0f;
		float f = h - floor(h);
		float p = brightness * (1.0f - saturation);
		float q = brightness * (1.0f - saturation * f);
		float t = brightness * (1.0f - (saturation * (1.0f - f)));
		switch ((int) h) {
		case 0:
			r = (int) (brightness * 255.0f + 0.5f);
			g = (int) (t * 255.0f + 0.5f);
			b = (int) (p * 255.0f + 0.5f);
			break;
		case 1:
			r = (int) (q * 255.0f + 0.5f);
			g = (int) (brightness * 255.0f + 0.5f);
			b = (int) (p * 255.0f + 0.5f);
			break;
		case 2:
			r = (int) (p * 255.0f + 0.5f);
			g = (int) (brightness * 255.0f + 0.5f);
			b = (int) (t * 255.0f + 0.5f);
			break;
		case 3:
			r = (int) (p * 255.0f + 0.5f);
			g = (int) (q * 255.0f + 0.5f);
			b = (int) (brightness * 255.0f + 0.5f);
			break;
		case 4:
			r = (int) (t * 255.0f + 0.5f);
			g = (int) (p * 255.0f + 0.5f);
			b = (int) (brightness * 255.0f + 0.5f);
			break;
		case 5:
			r = (int) (brightness * 255.0f + 0.5f);
			g = (int) (p * 255.0f + 0.5f);
			b = (int) (q * 255.0f + 0.5f);
			break;
		}
	}
	rgb_data data;
	data.r = (unsigned char) r;
	data.g = (unsigned char) g;
	data.b = (unsigned char) b;
	return data;
}

// a modulus function that can handle negatives and decimals
float mod2(float value, float min, float max) {
	float size = max - min;
	if (size == 0)
		return max;
	if (size < 0)
		return max;
	if (value < min || value >= max) {
		float quot = (value - min) / size;
		quot -= std::floor(quot);
		value = (size * quot) + min;
	}
	return value;
}

// core function of the fractal generation thread
void generateFractalThread() {
	float fx, fy, a, b, aa, bb, twoab;
	int n, index;

	// generate every pixel
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			index = (x + y * width) * 4;

			// pixel location in complex plane
			fx = x * fractalWidth / width + fractalX;
			fy = y * fractalHeight / height + fractalY;

			a = fx;
			b = fy;

			// how long does it take for the complex number to escape a complex circle with a radius of 4
			for (n = 0; n < iterations; n++) {
				aa = a * a;
				bb = b * b;
				twoab = 2.0f * a * b;

				a = aa - bb + fx;
				b = twoab + fy;

				if (aa + bb > 16) {
					break;
				}
			}

			// color the pixel appropriately
			if (n < iterations) {
				rgb_data color = fromHSB(mod2(n * 3.3f, 0, 256.0f) / 256.0f,
						1.0f, mod2(n * 16.0f, 0, 256.0f) / 256.0f);
				data[index] = color.r;
				data[index + 1] = color.g;
				data[index + 2] = color.b;
				data[index + 3] = 0xFF;
			} else {
				data[index] = 0x0;
				data[index + 1] = 0x0;
				data[index + 2] = 0x0;
				data[index + 3] = 0xFF;
			}

			progress++;
		}
	}

	// done generating
	generating.store(false);

	uv_async_send(doneAsync);
}

// called by javascript
void generateFractal(const Nan::FunctionCallbackInfo<v8::Value> &info) {
	if (!generating.load()) {
		generating.store(true);

		// check argument types
		if (info.Length() < 7) {
			Nan::ThrowTypeError("Wrong number of arguments to generateFractal");
			return;
		}

		for (int i = 0; i < 7; i++) {
			if (!info[i]->IsNumber()) {
				Nan::ThrowTypeError("Wrong argument types");
				return;
			}
		}

		// reset progress
		progress.store(0);

		// get the args
		width = info[0]->Int32Value();
		height = info[1]->Int32Value();
		fractalWidth = info[2]->NumberValue();
		fractalHeight = info[3]->NumberValue();
		fractalX = info[4]->NumberValue() - fractalWidth / 2;
		fractalY = info[5]->NumberValue() - fractalHeight / 2;
		iterations = info[6]->Int32Value();

		// setup the buffer
		v8::Local<v8::Object> buf =
				Nan::NewBuffer(width * height * 4).ToLocalChecked();
		buffer.Reset(info.GetIsolate(), buf);
		data = node::Buffer::Data(buf);

		// start the generator thread
		t = new std::thread(generateFractalThread);
	}
}

// if javascript want's to know how far the generation has gotten
void getProgress(const Nan::FunctionCallbackInfo<v8::Value> &info) {
	v8::Local<v8::Object> res = Nan::New<v8::Object>();
	res->Set(Nan::New("progress").ToLocalChecked(),
			Nan::New<v8::Number>(progress.load()));
	res->Set(Nan::New("max").ToLocalChecked(),
			Nan::New<v8::Number>(width * height));
	res->Set(Nan::New("generating").ToLocalChecked(),
			Nan::New(generating.load()));
	info.GetReturnValue().Set(res);
}

// javascript can set the callback function through here
void setDoneCallback(const Nan::FunctionCallbackInfo<v8::Value> &info) {
	if (info.Length() < 1) {
		Nan::ThrowTypeError("Wrong number of arguments to setDoneCallback");
		return;
	}

	if (!info[0]->IsFunction()) {
		Nan::ThrowTypeError("Wrong argument types");
		return;
	}

	doneCallbackFunc.Reset(info.GetIsolate(), info[0].As<v8::Function>());
}

// called by the uv_async_send(doneAsync);
void doneCallback(uv_async_t *handle) {
	v8::Isolate *isolate = v8::Isolate::GetCurrent();
	v8::HandleScope scope(isolate);
	v8::Local<v8::Function> func = doneCallbackFunc.Get(isolate);
	v8::Local<v8::Value> args[] = { buffer.Get(isolate) };
	func->Call(isolate->GetCurrentContext(), func, 1, args);
	buffer.Reset();
}

// called by the node module stuff
void init(v8::Local<v8::Object> exports) {
	// register thread merging callback system
	doneAsync = new uv_async_t;
	uv_loop_t *loop = uv_default_loop();
	uv_async_init(loop, doneAsync, doneCallback);

	// add functions to module's exports
	exports->Set(Nan::New("generateFractal").ToLocalChecked(),
			Nan::New<v8::FunctionTemplate>(generateFractal)->GetFunction());
	exports->Set(Nan::New("getProgress").ToLocalChecked(),
			Nan::New<v8::FunctionTemplate>(getProgress)->GetFunction());
	exports->Set(Nan::New("setDoneCallback").ToLocalChecked(),
			Nan::New<v8::FunctionTemplate>(setDoneCallback)->GetFunction());
}

NODE_MODULE(threads_test_native, init)
