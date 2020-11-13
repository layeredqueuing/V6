(TeX-add-style-hook "grammar"
 (lambda ()
    (LaTeX-add-index-entries
     "throughput!bounds"
     "rendezvous!delay"
     "rendezvous!variance"
     "send-no-reply!delay"
     "send-no-reply!variance"
     "open arrival!loss probability"
     "join!delay"
     "service time"
     "service time!variance"
     "service time!exceeded"
     "service time!distribution"
     "throughput"
     "utilization!task"
     "open arrival!waiting time"
     "waiting time!open arrival"
     "utilization!processor|textbf"
     "queueing time!processor|textbf")
    (LaTeX-add-labels
     "sec:old-grammar"
     "sec:input-file-bnf"
     "sec:output-file-bnf"
     "sec:general-p"
     "sec:bounds"
     "sec:rendezvous-delay-p"
     "sec:rendezvous-variance-p"
     "sec:send-no-reply-wait-p"
     "sec:send-no-reply-variance-p"
     "sec:arrival-loss-p"
     "sec:join-delay-p"
     "sec:service-time-p"
     "sec:service-time-variance-p"
     "sec:service-time-exceeded-p"
     "sec:service-time-distribution-p"
     "sec:througput-utilization-p"
     "sec:open-wait-p"
     "sec:processor-wait-utilization-p")
    (TeX-run-style-hooks
     "input-grammar")))

