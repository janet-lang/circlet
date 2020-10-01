# circlet

Circlet is an HTTP and networking library for the
[janet](https://github.com/janet-lang/janet) language.
It provides an abstraction out of the box like Clojure's
[ring](https://github.com/ring-clojure/ring), which is a server abstraction
that makes it easy to build composable web applications.

Circlet uses [mongoose](https://cesanta.com/) as the underlying HTTP server
engine. Mongoose is a portable, low footprint, event based server library. The
flexible build system requirements of mongoose make it very easy to embed
in other C programs and libraries.

## Installing

You can add Circlet as a dependency in your `project.janet`:

```clojure (declare-project
  :name "web framework" :description "A framework for web development"
  :dependencies ["https://github.com/janet-lang/circlet.git"])
```

You can also install it system-wide with `jpm`:

```sh jpm install circlet ```

## Usage

### Creating a server

You can create a HTTP server using the `circlet/server` function. The
function is of the form `(circlet/server handler port &opt ip-address)`
and takes the following parameters:

- `handler` function that takes the incoming HTTP request object (explained in
  greater detail below) and returns the HTTP response object.
- `port` number of the port on which the server will listen for incoming
  requests.
- `ip-address` optional string representing the IP address on which the server
  will listen (defaults to `“127.0.0.1”`). The address `“*”` will
  cause the server to listen on all available IP addresses.

The server runs immediately after creation.

### Request table

The `handler` function takes a single parameter representing the request. The
request is a Janet table containing all the information about the request. It
contains the following keys:

- `:uri` requested URI - `:method` HTTP method of the request as a Janet
string (e.g. "GET", "POST") - `:protocol` version of the HTTP protocol
used for request - `:headers` HTTP headers sent with the request as a Janet
table. Keys in this
  table are Janet strings with standard header's name (e.g. "Host",
  “Accept").  Values are the values in the HTTP header.
- `:body` body of the HTTP request - `:query-string` query string part of the
requested URI - `:connection` internal mongoose connection serving this request


### Response table

The return value of the `handler` function must be a Janet table containing
at least the `status` key with an integer value that corresponds to the HTTP
status of the response (e.g. 200 for success).

Other possible keys include:

- `:body` the body of the HTTP response (e.g. a string in HTML or JSON) -
`:headers` a Janet table or struct with standard HTTP headers. The structure is
  the same as the HTTP request case described above.

### Middleware

Circlet also allows for the creation of different “middleware”. Pieces
of middleware can be thought of as links in a chain of functions that are
used to consume the HTTP request. The `handler` function can be thought of
as a piece of middleware and other middleware should match the signature and
return type of the `handler` function, i.e. accept and return a Janet table.

Middleware can be created in one of two ways. A user can define a function
with the appropriate signature and return type or use Circlet’s
`circlet/middleware` function to coerce an argument into a piece of
middleware. Middleware pieces are often higher-order functions (meaning
that they return another function). This allows for parameterization at
creation time.

#### Provided middleware

There are three basic pieces of middleware provided by Circlet:

- `(circlet/router routes)` simple routing facility. This function takes
a Janet
  table containing the routes. Each key should be a Janet string matching
  a URI (e.g. `”/“`, `”/posts"`) with a value that is a function of
  the same form as the `handler` function described above.
- `(circlet/logger nextmw)` simple logging facility. This function prints the
  request info on `stdout`. The only argument is the next middleware.
- `(circlet/cookies nextmw)` middleware which extracts the cookies from
the HTTP
  header and stores the value under the `:cookies` key in the request object.

## Example

The below example starts a very simple web server on port 8000.

```clojure
(import circlet)

(defn myserver
 "A simple HTTP server" [request] {:status 200
  :headers {"Content-Type" "text/html"} :body "<!doctype
  html><html><body><h1>Hello.</h1></body></html>"})

(circlet/server myserver 8000)
```

## Development

### Building

Building requires [Janet](https://github.com/janet-lang/janet) to be installed
on the system, as well as the `jpm` tool (installed by default with Janet).

```sh jpm build ```

You can also just run `jpm` to see a list of possible build commands.

### Testing

Run a server on localhost with the following command

```sh jpm test ```



## License

Unlike [janet](https://github.com/janet-lang/janet), circlet is licensed
under the GPL license in accordance with mongoose.
