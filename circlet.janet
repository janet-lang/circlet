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
