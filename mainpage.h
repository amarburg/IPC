/**
\mainpage Table of Contents
-# \ref README.md
-# \ref page1
-# \ref page2

\page page1 Message based communication
IPC library provides two inter process communication methods: message exchange and RPC. Second method is implemented as a set of classes that wrap messange exchange to provide more simple way to call remote service function and process callbacks, but it lucks some flexability of message exchange.
This methods can be mixed to archive flexablity and simplicity in the same time. Important note: error handling in IPC is exception based.

To start use of IPC library by message based method include <i>ipc.hpp</i> to your source and add <i>ipc.cpp</i> to your project (for both server and client). Lets see server side first.

\section server Server application

First of all we must create listening socket, let it be an instance of ipc::unix_server_socket.

\code{.cpp}
    ipc::unix_server_socket server_socket("foo");
\endcode

Now we are ready to process incoming connections, but first we will discuss two message classes: ipc::in_message and ipc::out_message. This classes allows us 'out of the box' to serialize several 'primitive' types in stream manner. This 'primitive' types are:
<i>uint32_t, int32_t, uint64_t, int64_t, double, char, ipc::Message::RemotePtr, <b>string type</b> and <b>blob type</b></i>. <b>String type</b> may be any <i>std::string_view</i> compatible type (null termination is not required) for serializing and <i>std::string</i> for deserializing.
<b>Blob type</b> is <i>std::pair<const uint8_t*, size_t></i> for serializing and <i>std::vector<uint8_t></i> or <i>std::pair<std::array<uint8_t, N>size_t></i> for deserializing. To serialize/deserialize custom data structures we should overload stream operators, here is example:

\code{.cpp}
struct my_custom_type
{
    uint32_t a;
    int32_t  b;
};

ipc::in_message& operator <<(ipc::in_message& in, const my_custom_type& arg)
{
    return (in << arg.a << arg.b);
}

ipc::out_message& operator >>(ipc::out_message& out, const my_custom_type& arg)
{
    return (out >> arg.a >> arg.b);
}
\endcode

Second part of minimal server application is accept loop (fragment includes passive socket creation):

\code{.cpp}
try
{
    ipc::unix_server_socket server_socket("foo"); // has already been described
    
    auto predicate = []() { return !stop; };
    while (!stop)
    {
        auto p2p = server_socket.accept(predicate);
        
        try
        {
            ipc::in_message in;
            p2p.read_message(in, predicate);
            
            std::string req;
            in >> req;
            
            ipc::out_message out;
            out << req << " processed";
            
            if (!p2p.write_message(out, predicate))
                continue;
            
            p2p.wait_for_shutdown(predicate);   
        }
        catch (std::exception& ex)
        {
            std::cout << "request error >> " << ex.what() << std::endl;
        }
    }
}
catch (const ipc::user_stop_request_exception&) {}
catch (const std::exception& ex)
{
    std::cout << "fatal error >> " << ex.what() << std::endl;
}
\endcode

That's all about server, lets see client application.

\section clien Client application.
Client application is even more simple. We should connect to server and interract with it:

\code{.cpp}
try
{
    ipc::unix_client_socket client_socket("foo");
    ipc::out_message out;
    const char * req_text = "request";
    out << req_text;
    
    auto predicate = []() { return true; };
    client_socket.write_message(out, predicate);
    
    ipc::in_message in;
    client_socket.read_message(in, predicate);
    
    std::string resp;
    in >> resp;
    
    std::cout << req_text << " ->" << resp << std::endl;
}
catch(const std::exception& ex) 
{
    std::cout << "error >> " << ex.what() << std::endl;
}
\endcode

That's all about message based communication for now. For more info you can see <i>examples/simple-message-server.cpp</i> and <i>examples/simple-message-client.cpp</i>.

\page page2 RPC based communication
To start use of IPC library by RPC based method include <i>rpc.hpp</i> to your source and add <i>ipc.cpp</i> to your project (for both server and client). In addition to this files I recommend you to create additional header with functions and callbacks identification enumerations.

\section server Server application
Now we will discuss server example. It implements two methods of addition: with and without callbacks. Main part of RPC server - running ipc::rpc_server instance. Lets create and run it:

\code{.cpp}
try
{
    std::setlocale(LC_ALL, "");
    install_signal_handlers(); // we should stop if user press Crtl-C for example 

    ipc::rpc_server server("foo");
    server.run(dispatcher(), predicate);
    
    std::cout << "good bye" << std::endl;
}
catch(const std::exception& ex) 
{
    if (!g_stop)
        std::cout << "fatal error >> " << ex.what() <<std::endl;
} 
    
\endcode

dispatcher here is class that implements three callbacks: 
-# \code{.cpp}void invoke(uint32_t, ipc::in_message&, ipc::out_message&, ipc::point_to_point_socket&) const \endcode
-# \code{.cpp}void report_error(const std::exception&) const \endcode
-# \code{.cpp}void ready() const \endcode

First of it is used to invoke requested service (identifier is a first parameter), second is used to report errors and third is used to report "ready" state. Now we will see main part of server - dispatcher:

\code{.cpp}
class dispatcher
{
public:
    void invoke(uint32_t id, ipc::in_message& in_msg, ipc::out_message& out_msg, ipc::point_to_point_socket& p2p_socket) const
    {
        switch ((simple_server_function_t)id)
        {
        case simple_server_function_t::add_with_callbacks:
            ipc::function_invoker<int32_t(ipc::message::remote_ptr<true>), true>()(in_msg, out_msg, [&p2p_socket, &in_msg, &out_msg](const ipc::message::remote_ptr<true>& p) mutable -> int32_t {
                int32_t arg1 = ipc::service_invoker().call_by_channel<(uint32_t)simple_client_function_t::arg1, int32_t>(p2p_socket, in_msg, out_msg, predicate, p); // call remote callbacks
                int32_t arg2 = ipc::service_invoker().call_by_channel<(uint32_t)simple_client_function_t::arg2, int32_t>(p2p_socket, in_msg, out_msg, predicate, p);

                return arg1 + arg2;
                });
            break;
        case simple_server_function_t::add:
            ipc::function_invoker<int32_t(int32_t, int32_t), true>()(in_msg, out_msg, [](int32_t arg1, int32_t arg2) -> int32_t { return arg1 + arg2; }); // just call native function
            break;
        default:
            break;
        }
    }

    void report_error(const std::exception& ex) const
    {
        if (!g_stop)
            std::cout << "call error >> " << ex.what() << std::endl;
    }

    void ready() const
    {
        std::cout << "server is ready" << std::endl;
    }
};
\endcode

Lets see ipc::function_invoker and ipc::service_invoker a bit closer. First of it used to call native function (service on server and callback on client): it "extracts" function arguments from incoming message, calls user function (or functor) and "packs" result to outcomming message. 
ipc::service_invoker do another thing: it calles remote service (service on client and callback on server): it "packs" user provided data to outcomming message and "extracts" it from incomming. And, of course, both object send and receive messages. Important note: implementation drops cv and reference modifiers in template function description (for example <i>ipc::function_invoker<int32_t(ipc::message::remote_ptr<true>), true></i> has the same meaning as <i>ipc::function_invoker<int32_t(const ipc::message::remote_ptr<true>&), true></i>). Second template parameter controls termination tag inserting and should be true for service implementation on server and false for callbacks on client.

There is only one case where user have to work with messages directly in RPC based library use: custom types serializing and deserializing. Simple example of it you can see on previous page.

\section client Client application
Now it is time to see client source:

\code{.cpp}
// context for callback based service
struct add_args
{
    int32_t a;
    int32_t b;
};

static bool dispatch(uint32_t id, ipc::in_message& in_msg, ipc::out_message& out_msg)
{
    switch ((simple_client_function_t)id)
    {
    case simple_client_function_t::arg1:
        ipc::function_invoker<int32_t(ipc::message::remote_ptr<true>), false>()(in_msg, out_msg, [](const ipc::message::remote_ptr<true>& p) { return ((const add_args*)p.get_pointer())->a; });
        return true;
    case simple_client_function_t::arg2:
        ipc::function_invoker<int32_t(ipc::message::remote_ptr<true>), false>()(in_msg, out_msg, [](const ipc::message::remote_ptr<true>& p) { return ((const add_args*)p.get_pointer())->b; });
        return true;
    default:
        return false;
    }
}

static bool minimal_dispatch(uint32_t id, ipc::in_message& in_msg, ipc::out_message& out_msg) // just stub
{
    return false;
}

static auto minimal_predicate = []() { return true; }; //bad practice, just an example

int main()
{
    try
    {
        std::setlocale(LC_ALL, "");

        add_args args = { 3, 4 };
        auto result = ipc::service_invoker().call_by_link<(uint32_t)simple_server_function_t::add_with_callbacks, int32_t>("foo", dispatch, minimal_predicate, ipc::message::remote_ptr<true>(&args));
        std::cout << "add(" << args.a << ", " << args.b << ") = " << result << std::endl;

        int32_t a = 7, b = 8;
        result = ipc::service_invoker().call_by_link<(uint32_t)simple_server_function_t::add, int32_t>("foo", minimal_dispatch, minimal_predicate, a, b);
        std::cout << "add(" << a << ", " << b << ") = " << result << std::endl;

        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cout << "error >> " << ex.what() << std::endl;
        return 1;
    }
}
\endcode

ipc::service_invoker().call_by_link requires dispatch function (or functor): it handles callbacks from server to client. If there is no callback this routine can return false for any request, but is better to check identifier for ipc::function_invoker_base::done_tag equality and process any other code as error.

That's all about RPC based communication for now. For more info you can see <i>examples/simple-rpc-server.cpp</i> and <i>examples/simple-rpc-client.cpp</i>. They have the same functionality as message based samples and can be swapped with them.

*/
