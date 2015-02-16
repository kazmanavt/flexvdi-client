/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#ifndef _FLEXVDIAGENT_HPP_
#define _FLEXVDIAGENT_HPP_

#include <memory>
#include <functional>
#include <boost/asio.hpp>
#include "DispatcherRegistry.hpp"
#include "VirtioPort.hpp"
#include "LocalPipe.hpp"

namespace flexvm {

class FlexVDIGuestAgent {
public:
    static FlexVDIGuestAgent & singleton() {
        static FlexVDIGuestAgent instance;
        return instance;
    }

    int run();
    void stop();
    void setVirtioEndpoint(const std::string & name) {
        port.setEndpoint(name);
    }
    void setLocalEndpoint(const std::string & name) {
        pipe.setEndpoint(name);
    }
    Connection::Ptr spiceClient() {
        return port.spiceClient();
    }
    DispatcherRegistry & getDispatcherRegistry() { return dregistry; }

private:
    FlexVDIGuestAgent() :
    port(io, dregistry.asMessageHandler()),
    pipe(io, dregistry.asMessageHandler()) {}

    boost::asio::io_service io;
    DispatcherRegistry dregistry;
    VirtioPort port;
    LocalPipe pipe;
};


#define REGISTER_COMPONENT(Component) \
REGISTER_COMPONENT_WITH_DISPATCHER(Component, \
    FlexVDIGuestAgent::singleton().getDispatcherRegistry())

} // namespace flexvm

#endif // _FLEXVDIAGENT_HPP_
