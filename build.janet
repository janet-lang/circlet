(import cook)

(cook/make-native
    :name "circletc"
    :source @["circletc.c" "mongoose.c"])
