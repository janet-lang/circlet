# circlet

Circlet is an HTTP and networking library for the [janet](https://github.com/bakpakin/janet) language.
It provides an abstraction out of the box like Clojure's [ring](https://github.com/ring-clojure/ring), which
is a server abstraction that makes it easy to build composable web applications.

Circlet uses [mongoose](https://cesanta.com/) as the underlying HTTP server engine. Mongoose
is a portable, low footprint, event based server library. The flexible build system requirements
of mongoose make it very easy to embed in other C programs and libraries.

# Building

Building requires [janet](https://github.com/bakpakin/janet) to be installed on the system.

On Linux and macos systems, just run `make` to build, and `make install` to install to
the configured janet path (`JANET_PATH`).

# Example

The below example starts a very simple web server on port 8000.

```lisp
(import circlet)

(defn myserver 
 "A simple HTTP server"
 [req]
 {:status 200
  :headers {"Content-Type" "text/html"}
  :body "<!doctype html><html><body><h1>Hello.</h1></body></html>"})

(circlet.server 8000 myserver)
```

# License

Unlike [janet](https://github.com/bakpakin/janet), circlet is licensed under
the GPL license in accordance with mongoose. 
