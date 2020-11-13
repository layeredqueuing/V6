(TeX-add-style-hook "userman"
 (lambda ()
    (LaTeX-add-index-entries
     "-#1@\\texttt{-#1}"
     "--#1@\\texttt{--#1}"
     "#1@\\textit{#1}"
     "#1"
     "error!#1"
     "#1!error"
     "element!#1"
     "attribute!#1"
     "#1@\\texttt{#1}"
     "type!#1"
     "#1@\\textbf{#1}"
     "Layered Queueing Network"
     "queueing network!extended"
     "queueing network!layered"
     "resource!possession")
    (LaTeX-add-bibliographies)
    (TeX-add-symbols
     '("schematype" 1)
     '("attribute" 1)
     '("schemaelement" 1)
     '("indexerror" 1)
     '("manpage" 2)
     '("pragma" 1)
     '("optarg" 2)
     '("longopt" 1)
     '("flag" 2))
    (TeX-run-style-hooks
     "hyperref"
     "multicol"
     "listings"
     "color"
     "rotating"
     "makeidx"
     "subfig"
     "moreverb"
     "verbatim"
     "dcolumn"
     "epsf"
     "bnf"
     "times"
     "latex2e"
     "rep10"
     "report"
     "model"
     "results"
     "schema"
     "lqx"
     "lqns"
     "lqsim"
     "errors"
     "defects"
     "grammar")))

