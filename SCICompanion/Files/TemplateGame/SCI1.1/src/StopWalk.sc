(version 2)
(include "sci.sh")
(exports
    0 StopWalk
)
(use "Cycle")
(script 961)



(class public StopWalk of Fwd
    (properties
        client 0
        caller 0
        cycleDir 1
        cycleCnt 0
        completed 0
        vWalking 0
        vStopped 0
    )

    (method (init theClient theVStopped)
        (if (paramTotal)
            = vWalking (send ((= client theClient)):view)
            (if (>= paramTotal 2)
                = vStopped theVStopped
            )
        )
        (super:init(client))
        (self:doit())
    )


    (method (doit)
        (var clientLoop, temp1)
        (if ((send client:isStopped()))
            (if ((== vStopped -1) and (<> (send client:loop) (- NumLoops(client) 1)))
                = clientLoop (send client:loop)
                (super:doit())
                (send client:
                    loop(- NumLoops(client) 1)
                    setCel(clientLoop)
                )
            )(else
                (if ((<> vStopped -1) and (== (send client:view) vWalking))
                    (send client:view(vStopped))
                    (super:doit())
                )(else
                    (if (<> vStopped -1)
                        (super:doit())
                    )
                )
            )
        )(else
            (switch (vStopped)
                (case (send client:view)
                    (send client:view(vWalking))
                )
                (case -1
                    (send client:
                        setLoop(-1)
                        setCel(-1)
                    )
                    (if (== (send client:loop) (- NumLoops(client) 1))
                        (send client:
                            loop((send client:cel))
                            cel(0)
                        )
                    )
                )
            )
            (super:doit())
        )
    )


    (method (dispose)
        (if (== (send client:view) vStopped)
            (send client:view(vWalking))
        )
        (super:dispose())
    )

)
