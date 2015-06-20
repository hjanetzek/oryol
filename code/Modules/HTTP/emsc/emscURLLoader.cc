//------------------------------------------------------------------------------
//  emscURLLoader.cc
//------------------------------------------------------------------------------
#include "Pre.h"
#include "emscURLLoader.h"
#include "IO/Stream/MemoryStream.h"
#include <emscripten/emscripten.h>

namespace Oryol {
namespace _priv {

//------------------------------------------------------------------------------
// FIXME FIXME FIXME
// Properly handle cancelled messages.
void
emscURLLoader::doWork() {
    while (!this->requestQueue.Empty()) {
        this->startRequest(this->requestQueue.Dequeue());
    }
}

//------------------------------------------------------------------------------
void
emscURLLoader::startRequest(const Ptr<HTTPProtocol::HTTPRequest>& req) {
    o_assert(req.isValid() && !req->Handled());

    // currently only support GET
    o_assert(req->GetMethod() == HTTPMethod::Get);

    // bump the requests refcount and get a raw pointer
    HTTPProtocol::HTTPRequest* reqPtr = req.get();
    reqPtr->addRef();

    // start the asynchronous XHR
    // NOTE: we can only load from our own HTTP server, so the host part of 
    // the URL is completely irrelevant...
    //String urlPath = req->GetURL().PathToEnd();
    auto urlPath = req->GetURL();
    emscripten_async_wget_data(urlPath.AsCStr(), (void*) reqPtr, emscURLLoader::onLoaded, emscURLLoader::onFailed);
}

//------------------------------------------------------------------------------
void
emscURLLoader::onLoaded(void* userData, void* buffer, int size) {
    o_assert(0 != userData);
    o_assert(0 != buffer);
    o_assert(size > 0);

    // user data is a HTTPRequest ptr, put it back into a smart pointer
    Ptr<HTTPProtocol::HTTPRequest> req = userData;
    req->release();

    // create a HTTPResponse and fill it out
    Ptr<HTTPProtocol::HTTPResponse> response = HTTPProtocol::HTTPResponse::Create();
    response->SetStatus(IOStatus::OK);

    // write response body
    const Ptr<IOProtocol::Request>& ioReq = req->GetIoRequest();
    Ptr<MemoryStream> responseBody = MemoryStream::Create();
    responseBody->SetURL(req->GetURL());
    responseBody->Open(OpenMode::WriteOnly);
    responseBody->Write(buffer, size);
    responseBody->Close();
    response->SetBody(responseBody);

    // set the response on the request, mark the request as handled
    // also fill the embedded IORequest object
    req->SetResponse(response);
    if (ioReq) {
        auto httpResponse = req->GetResponse();
        ioReq->SetStatus(httpResponse->GetStatus());
        ioReq->SetStream(httpResponse->GetBody());
        ioReq->SetErrorDesc(httpResponse->GetErrorDesc());
        ioReq->SetHandled();
    }
    req->SetHandled();
}

//------------------------------------------------------------------------------
void
emscURLLoader::onFailed(void* userData) {
    o_assert(0 != userData);

    // user data is a HTTPRequest ptr, put it back into a smart pointer
    Ptr<HTTPProtocol::HTTPRequest> req = userData;
    req->release();
    Log::Dbg("emscURLLoader::onFailed(url=%s)\n", req->GetURL().AsCStr());

    // hmm we don't know why it failed, so make something up, we should definitely
    // fix this somehow (looks like the wget2 functions also pass a HTTP status code)
    const IOStatus::Code ioStatus = IOStatus::NotFound;
    Ptr<HTTPProtocol::HTTPResponse> response = HTTPProtocol::HTTPResponse::Create();
    response->SetStatus(ioStatus);
    req->SetResponse(response);
    auto ioReq = req->GetIoRequest();
    if (ioReq) {
        auto httpResponse = req->GetResponse();
        ioReq->SetStatus(httpResponse->GetStatus());
        ioReq->SetHandled();
    }
    req->SetHandled();
}

} // namespace _priv
} // naespace Oryol
