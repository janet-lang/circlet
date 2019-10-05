(import build/circlet :as circlet)

# Now build our server
(circlet/server 
  (->
      {"/thing" {:status 200
                 :headers {"Content-Type" "text/html; charset=utf-8"}
                 :body "<!doctype html><html><body>
                    <h1>Is a thing.</h1>
                    <form action=\"bork\">
                      <input type=\"text\" name=\"firstname\">
                      <input type=\"submit\" value=\"Submit\">
                    </form>
                  </body></html>"}
       "/redirect" {:status 302
                    :headers {"Location" "/thing"}}
       :default {:kind :static
                 :root "."}}
      circlet/router
      circlet/logger)
  8000)
