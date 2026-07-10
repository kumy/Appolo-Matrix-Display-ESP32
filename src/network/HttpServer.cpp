#include "network/HttpServer.h"

void HttpServer::begin() {
  running_ = true;
}

void HttpServer::poll() {}

bool HttpServer::running() const {
  return running_;
}
