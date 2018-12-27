# This is embedded, so all circlet functions are available

(defn middleware
  "Coerce any type to http middleware"
  [x]
  (case (type x)
    :function x
    (fn [&] x)))

(defn router
  "Creates a router middleware"
  [routes]
  (fn [req] 
    (def r (or
             (get routes (get req :uri))
             (get routes :default)))
    (if r ((middleware r) req) 404)))

(defn logger
  "Creates a logging middleware"
  [nextmw]
  (fn [req]
    (def {:uri uri
          :protocol proto
          :method method
          :query-string qs} req)
    (def start-clock (os/clock))
    (def start-time (os/time))
    (def ret (nextmw req))
    (def end-clock (os/clock))
    (def fulluri (if (< 0 (length qs)) (string uri "?" qs) uri))
    (def elapsed (string/number (* 1000 (- end-clock start-clock)) :f 3))
    (def status (or (get ret :status) 200))
    (print proto " " method " " status " " fulluri  " at " start-time " elapsed " elapsed "ms")
    ret))

(defn server 
  "Creates a simple http server"
  [port handler]
  (def mgr (manager))
  (def mw (middleware handler))
  (defn evloop []
    (print "Circlet server listening on port " port "...")
    (var req (yield nil))
    (while true
      (set req (yield (mw req)))))
  (bind-http mgr (string port) evloop)
  (while true (poll mgr 2000)))
