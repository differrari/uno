(load "~/redbuild/v3/redbuild.lisp")

(redbuild:set-tester "ui.c")

(redbuild:quick-build (redbuild:make-instance `redbuild:redmod
        :name "uno"
        :type (redbuild:dynlib)
        :target (redbuild:native)
        :srcs (redbuild:dynsrc "doc.c" "text.c" "uno.c")
) :add-dependencies t :run t :success (lambda () (print (redbuild:emit-compile-commands))))