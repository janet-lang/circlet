(import build/circlet :as circlet)

# Now build our server
(circlet/server
  (->
    {"/thing" {:status 200
               :headers {"Content-Type" "text/html; charset=utf-8"
                         "Thang" [1 2 3 4 5]}
               :body "<!doctype html><html><body>
                    <h1>Is a thing.</h1>
                    <form action=\"bork\">
                      <input type=\"text\" name=\"firstname\">
                      <input type=\"submit\" value=\"Submit\">
                    </form>
                  </body></html>"}
     "/bork" (fn [req]
               (let [[fname] (peg/match '(* "firstname=" (<- (any 1)) -1) (req :query-string))]
                 {:status 200 :body (string "<!doctype html><html><body>Your firstname is "
                                            fname "?</body></html>")}))
     "/blob" {:status 200
              :body @"123\0123"}
     "/redirect" {:status 302
                  :headers {"Location" "/thing"}}
     "/readme" {:kind :file :file "README.md" :mime "text/plain"}
     :default {:kind :static
               :root "."}}
    circlet/router
    circlet/logger)
  8000)
