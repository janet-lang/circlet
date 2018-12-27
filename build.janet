(import cook)

(cook/make-native
    :name "circlet"
    :embedded @["circlet_lib.janet"]
    :source @["circlet.c" "mongoose.c"])
