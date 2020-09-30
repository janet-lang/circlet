# circlet

Circlet is an HTTP and networking library for the [janet](https://github.com/janet-lang/janet) language.
It provides an abstraction out of the box like Clojure's [ring](https://github.com/ring-clojure/ring), which
is a server abstraction that makes it easy to build composable web applications.

Circlet uses [mongoose](https://cesanta.com/) as the underlying HTTP server engine. Mongoose
is a portable, low footprint, event based server library. The flexible build system requirements
of mongoose make it very easy to embed in other C programs and libraries.

## Installing

You can add circlet as dependency in your `project.janet`:

```clojure
(declare-project
  :name "web framework"
  :description "A framework for web development"
  :dependencies ["https://github.com/janet-lang/circlet.git"])
```

You can also install it system-wide with `jpm`:

```sh
jpm install circlet
```

## Usage

### Creating a server

Library provides means to create http server with `circlet/server` function with
signarure: `(circlet/server handler port &opt ip-address)`. Parameters are:

- `handler` function which will handle the incoming http request table, and
returns http response table. See below.
- `port` TCP port on which will the server listen for upcomming requests
- `ip-address` optional IP address on which server will listen. When not
provided defaults to 127.0.0.1. It can be * and server will then listen on all
available IP addresses

Server runs immediately after creation on given host and port.

### Request table

The only argument, which `handler` function from previous section accepts is
the request. It is Janet `table` containing all the information about the
request. It contains following keys:

- `:uri` requested URI
- `:method` HTTP method of the request as a Janet string (e.g. "GET", "POST")
- `:protocol` version of the HTTP protocol used for request
- `:headers` Janet table containing all HTTP headers sent with the request. Keys
 in this table are Janet strings with standard header's name (e.g. "Host",
"Accept"). Values are the HEADER content.
- `:body` body of the HTTP request
- `:query-string` query string part of the requested URI
- `:connection` internal mongoose connection serving this request


### Response table

Return value of the `handler` function must be Janet `table` containing at least
the `status` key corresponding to HTTP status of the response (e.g. 200 for the
success).

Other keys could be:

- `:body` the body of the HTTP response (e.g. HTML code for the page, JSON doc)
- `:headers` a Janet `table` or `struct` with standard HTTP headers. Structure
is the same as in HTTP request case above.

### Midlewares

The last functionality that Circlet provides is creating middlewares. You can
think about middlewares as chain for consuming the HTTP request. You can look at
the `handle` function as a middleware.

Circlet provides `circlet/middleware` function which coerces any function or
table to middleware. Signature and the return value of the middleware must be the same
as for the `handler` function. Midlewares are often higher order functions
(meaning they return another function) for parametrization at their create time.

#### Provided middlewares

There are three basic middlewares provided by the Circlet:

- `(circlet/router routes)` simple routing facility. This function takes Janet
table containing the routes. Each key should be Janet string with URI (e.g.
"/", "/posts") and its value `handler` function same as in the text above.
- `(circlet/logger nextmw)` simple loggin facility. It just prints the request
info on `stdout`. The only argument is the next middleware. It is usually
`handler` function or the `router` middleware.
- `(circlet/cookies nextmw)` middleware which extracts the cookies from the HTTP
header and puts it to request parameter under `:cookies` key.

## Example

The below example starts a very simple web server on port 8000.

```clojure
(import circlet)

(defn myserver
 "A simple HTTP server"
 [request]
 {:status 200
  :headers {"Content-Type" "text/html"}
  :body "<!doctype html><html><body><h1>Hello.</h1></body></html>"})

(circlet/server myserver 8000)
```

## Development

### Building

Building requires [Janet](https://github.com/janet-lang/janet) to be installed on the system, as
well as the `jpm` tool (installed by default with Janet).

```sh
jpm build
```

You can also just run `jpm` to see a list of possible build commands.

### Testing

Run a server on localhost with the following command

```sh
jpm test
```



## License

Unlike [janet](https://github.com/janet-lang/janet), circlet is licensed under
the GPL license in accordance with mongoose.
