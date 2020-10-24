(import build/circlet :as circlet)

(def tests [
    ["" @{}]
    ["abc=5&%20+=" @{"abc" "5" "  " ""}]
    ["a=b" @{"a" "b"}]
])

(each tc tests
  (def r (circlet/parse-x-www-form-urlencoded (tc 0)))
  (unless (deep= r (tc 1))
    (errorf "fail: %j != %j" r (tc 1))))
