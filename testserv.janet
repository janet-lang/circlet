(import circlet)

(def body
  ````<!doctype html>
<html>
<body>
  Nope.
</body>
</html>
````)

# Now build our server
(circlet.server 
  8000
  (circlet.router 
    {"/thing" {:status 200
               :headers {"Content-Type" "text/html"}
               :body "<!doctype html><html><body><h1>Is a thing.</h1></body></html>"}
     :default {:status 404
               :headers {"Content-Type" "text/html"}
               :body body}}))
