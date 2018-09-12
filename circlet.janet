# Generates code that can be serialized

(import circletc :as cc)

(defn middleware
  "Coerce any type to http middleware"
  [x]
  (case (type x)
    :function x
    (fn @[] x)))

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
    (def start-clock (os.clock))
    (def start-time (os.time))
    (def ret (nextmw req))
    (def end-clock (os.clock))
    (def fulluri (if (< 0 (length qs)) (string uri "?" qs) uri))
    (def elapsed (int (* 1000 (- end-clock start-clock))))
    (print proto " " method " " fulluri  " timestamp " start-time " elapsed " elapsed "ms")
    ret))

(defn server 
  "Creates a simple http server"
  [port handler]
  (def mgr (cc.manager))
  (def mw (middleware handler))
  (defn evloop [conn]
    (print "Circlet server listening on port " port "...")
    (var req (yield nil))
    (while true
      (:= req (yield (mw req)))))
  (cc.bind-http mgr (string port) evloop)
  (while true (cc.poll mgr 2000)))
