# TODO

* Batch optimization: Allow for varying batches during optimization -> SGD and variants
    - Set new initial condition before braid\_Drive(). 
    - If (!warm\_restart), init can not be called before drive() because the grid doesn't exist yet. 
      Hence: Before drive(), call setInitCondition(warm\_restart)
    - setInitCondition(warm\_restart) only writes initial condition if (!warm\_restart), for both primal and adjoint!

* Weights parametrization using splines



# Parameter study: Lessons Learned (n=32, peaks example)

* if **tanh** activation:
    - *Expand* input data to first layer using zeros
    - Weights initialization: 
         * opening layer:    zero
         * general layer:   random, factor 1e-3 or bigger
         * classif layer:   zero
    - Regularization param: 
         * tikhonov-term:    small, e.g. 1e-7
         * ddt-term:         small, e.g. 1e-7
         * class-term:       small, e.g. 1e-5, 1e-7

* if **ReLu** activation:
    - Use *opening layer* to map input data to first layer
    - Initialization:
         * opening layer:   random, factor 1e-3
         * general layer:   zero
         * classif layer:   random, factor 1e-3
    - Regularization:
         * tikhonov-term:    small, e.g. 1e-5 or 1e-7
         * ddt-term:         small, e.g. 1e-5 or 1e-7
         * class-term:       1e-3

* Rule of thumb for initialization: 
    loss(first iteration) = - log(P(GuessTheRightClass))

