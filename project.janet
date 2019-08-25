(declare-project
  :name "circlet"
  :description "HTTP server bindings for Janet."
  :author "Calvin Rose"
  :license "MIT"
  :url "https://github.com/janet-lang/circlet"
  :repo "git+https://github.com/janet-lang/circlet.git")

(declare-native
  :name "circlet"
  :embedded ["circlet_lib.janet"]
  :lflags (if (= :windows (os/which))
            # for now, assume 32 bit compilation.
            ["advapi32.lib"])
  :source ["circlet.c" "mongoose.c"])

(phony "update-mongoose" []
      (os/shell "curl https://raw.githubusercontent.com/cesanta/mongoose/master/mongoose.c > mongoose.c")
      (os/shell "curl https://raw.githubusercontent.com/cesanta/mongoose/master/mongoose.h > mongoose.h"))
