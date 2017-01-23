#include <nan.h>
#include <thread>

void generateFractal(const Nan::FunctionCallbackInfo<v8::Value> &info) {
  // TODO more stuff
}

void init(v8::Local<v8::Object> exports) {
  exports->Set(Nan::New("generateFractal").ToLocalChecked(),
    Nan::New<v8::FunctionTemplate>(generateFractal)->GetFunction());
}

NODE_MODULE(threads_test_native, init)
