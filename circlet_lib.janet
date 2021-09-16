# This is embedded, so all circlet functions are available

(defn middleware
  "Coerce any type to http middleware"
  [x]
  (case (type x)
    :function x
    (fn [&] x)))

(defn router
  "Creates a router middleware. Route parameter must be table or struct
  where keys are URI paths and values are handler functions for given URI"
  [routes]
  (fn [req]
    (def r (or
             (get routes (get req :uri))
             (get routes :default)))
    (if r ((middleware r) req) 404)))

(defn logger
  "Creates a logging middleware. nextmw parameter is the handler function
  of the next middleware"
  [nextmw]
  (fn [req]
    (def {:uri uri
          :protocol proto
          :method method
          :query-string qs} req)
    (def start-clock (os/clock))
    (def ret (nextmw req))
    (def end-clock (os/clock))
    (def fulluri (if (< 0 (length qs)) (string uri "?" qs) uri))
    (def elapsed (string/format "%.3f" (* 1000 (- end-clock start-clock))))
    (def status (or (get ret :status) 200))
    (print proto " " method " " status " " fulluri " elapsed " elapsed "ms")
    ret))

(defn cookies
  "Parses cookies into the table under :cookies key. nextmw parameter is
  the handler function of the next middleware"
  [nextmw]
  (def grammar
    (peg/compile
      {:content '(some (if-not (set "=;") 1))
       :eql "="
       :sep '(between 1 2 (set "; "))
       :main '(some (* (<- :content) :eql (<- :content) (? :sep)))}))
  (fn [req]
    (-> req
        (put :cookies
             (or (-?>> [:headers "Cookie"]
                       (get-in req)
                       (peg/match grammar)
                       (apply table))
                 {}))
        nextmw)))

(defn server
  "Creates a simple http server. handler parameter is the function handling the
  requests. It could be middleware. port is the number of the port the server
  will listen on. ip-address is optional IP address the server will listen on"
  [handler port &opt ip-address]
  (def mgr (manager))
  (def mw (middleware handler))
  (default ip-address "127.0.0.1")
  (def interface
    (if (peg/match "*" ip-address)
      (string port)
      (string/format "%s:%d" ip-address port)))
  (defn evloop []
    (print (string/format "Circlet server listening on [%s:%d] ..." ip-address port))
    (var req (yield nil))
    (while true
      (set req (yield (mw req)))))
  (bind-http mgr interface evloop)
  (while true (poll mgr 2000)))


(defn server-websocket
  "Creates a simple http+websocket server. handler parameter is the function handling the
  requests. It could be middleware. websocket-handler is the function handling websocket
  messages. port is the number of the port the server
  will listen on. ip-address is optional IP address the server will listen on"
  [handler websocket-handler port &opt ip-address]
  (def mgr (manager))
  (def mw (middleware handler))
  (def ws-mw (middleware websocket-handler))
  (default ip-address "127.0.0.1")
  (def interface
    (if (peg/match "*" ip-address)
      (string port)
      (string/format "%s:%d" ip-address port)))
  (defn evloop []
    (print (string/format "Circlet server listening on [%s:%d] ..." ip-address port))
    (var req (yield nil))
    (while true
      (case (req :protocol)
        "websocket"
        (set req (yield (ws-mw mgr req)))

        (set req (yield (mw req))))))
  (bind-http-websocket mgr interface evloop)
  (while true (poll mgr 2000)))
