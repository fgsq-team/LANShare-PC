//
// Created by fgsq on 2023/12/17.
//

#ifndef LANSHARE_HTTPHANDLER_H
#define LANSHARE_HTTPHANDLER_H


#include "Request.h"
#include "Response.h"

using HttpHandler = void (*)(Request *request, Response *response);
using RequestFilter = void (*)(Request *request, Response *response, HttpHandler httpHandler);

#endif //LANSHARE_HTTPHANDLER_H
