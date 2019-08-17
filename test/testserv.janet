(import build/circlet :as circlet)

# Now build our server
(circlet/server 
  (->
      {"/thing" {:status 200
                 :headers {"Content-Type" "text/html"}
                 :body "<!doctype html><html><body><h1>Is a thing.</h1></body></html>"}
       :default {:kind :static
                 :root "."}}
      circlet/router
      circlet/logger)
  8000)
