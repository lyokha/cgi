//                 -- server2/main.hpp --
//
//           Copyright (c) Darren Garvey 2007.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
////////////////////////////////////////////////////////////////
//
//[fcgi_server2
//
// This example simply echoes all variables back to the user. ie.
// the environment and the parsed GET, POST and cookie variables.
// Note that GET and cookie variables come from the environment
// variables QUERY_STRING and HTTP_COOKIE respectively.
//
// It is a demonstration of how a 'server' can be used to abstract
// away the differences between FastCGI and CGI requests.
//
// This is very similar to the fcgi_echo and fcgi_server1 examples.
// Unlike in the server1 example, the server class in this example uses
// asynchronous functions, to increase throughput.
//
//
// **FIXME**
// This is slower than the server1 example, which is stupid.

#include <fstream>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/program_options/environment_iterator.hpp>

#include <boost/cgi/fcgi.hpp>

using namespace std;
using namespace boost::fcgi;


// This function writes the title and map contents to the ostream in an
// HTML-encoded format (to make them easier on the eye).
template<typename OStream, typename Map>
void format_map(OStream& os, Map& m, const std::string& title)
{
  os<< "<h2>" << title << "</h2>";
  if (m.empty()) os<< "NONE<br />";
  for (typename Map::const_iterator i = m.begin(), end = m.end()
      ; i != end; ++i)
  {
    os<< "<b>" << i->first << "</b> = <i>" << i->second << "</i><br />";
  }
}

/// Handle one request and return.
/**
 * If it returns != 0 then an error has occurred. Sets ec to the error_code
 * corresponding to the error.
 */
int handle_request(fcgi::request& req, boost::system::error_code& ec)
  {
    // Construct a `response` object (makes writing/sending responses easier).
    response resp;

    // Responses in CGI programs require at least a 'Content-type' header. The
    // library provides helpers for several common headers:
    resp<< content_type("text/html")
    // You can also stream text to a response object. 
        << "Hello there, universe!<p />";

    // Use the function defined above to show some of the request data.
    format_map(resp, req[env_data], "Environment Variables");
    format_map(resp, req[get_data], "GET Variables");
    format_map(resp, req[cookie_data], "Cookie Variables");

    //log_<< "Handled request, handling another." << std::endl;

    // This funky macro finishes up:
    return_(resp, req, 0);
    // It is equivalent to the below, where the third argument is represented by
    // `program_status`:
    //
    // resp.send(req.client());
    // req.close(resp.status(), program_status);
    // return program_status;
    //
    // Note: in this case `program_status == 0`.
  }


/// The server is used to abstract away protocol-specific setup of requests.
/**
 * This server only works with FastCGI, but as you can see in the
 * request_handler::handle_request() function above, the request in there could
 * just as easily be a cgi::request.
 *
 * Later examples will demonstrate making protocol-independent servers.
 * (**FIXME**)
 */
class server
{
public:
  typedef fcgi::service                         service_type;
  typedef fcgi::acceptor                        acceptor_type;
  typedef fcgi::request                         request_type;
  typedef boost::function<
            int ( request_type&
                , boost::system::error_code&)
          >                                     function_type;

  server(const function_type& handler)
    : handler_(handler)
    , service_()
    , acceptor_(service_)
  {}

  void run(int num_threads = 0)
  {
    // Create a new request (on the heap - uses boost::shared_ptr<>).
    request_type::pointer new_request = request_type::create(service_);
    // Add the request to the set of existing requests.
    requests_.insert(new_request);

    start_accept(new_request);
    boost::thread_group tgroup;
    for(int i(num_threads); i != 0; --i)
    {
      tgroup.create_thread(boost::bind(&service_type::run, &service_));
    }
    tgroup.join_all();
  }

  void start_accept(request_type::pointer& new_request)
  {
    acceptor_.async_accept(*new_request, boost::bind(&server::handle_accept
                                                    , this, new_request
                                                    , boost::asio::placeholders::error)
                          );
  }

  void handle_accept(request_type::pointer req  
                    , boost::system::error_code ec)
  {
    if (ec)
    {
      //std::cerr<< "Error accepting request: " << ec.message() << std::endl;
      return;
    }

    req->load(ec, true);

    //req->async_load(boost::bind(&server::handle_request, this
    //                           , req, boost::asio::placeholders::error)
    //               , true);

    service_.post(boost::bind(&server::handle_request, this, req, ec));

    // Create a new request (on the heap - uses boost::shared_ptr<>).
    request_type::pointer new_request = request_type::create(service_);
    // Add the request to the set of existing requests.
    requests_.insert(new_request);

    start_accept(new_request);
  }

  void handle_request(request_type::pointer req
                     , boost::system::error_code ec)
  {
    handler_(*req, ec);
    if (ec)
    {
      //std::cerr<< "Request handled, but ended in error: " << ec.message()
      //         << std::endl;
    }
    start_accept(req);
  }

private:
  function_type handler_;
  service_type service_;
  acceptor_type acceptor_;
  std::set<request_type::pointer> requests_;
};

int main()
try
{
  server s(&handle_request);

  // Run the server with 10 threads handling the asynchronous functions.
  s.run(10);

  return 0;
  
}catch(boost::system::system_error& se){
  cerr<< "[fcgi] System error: " << se.what() << endl;
  return 1313;
}catch(exception& e){
  cerr<< "[fcgi] Exception: " << e.what() << endl;
  return 666;
}catch(...){
  cerr<< "[fcgi] Uncaught exception!" << endl;
  return 667;
}
//]