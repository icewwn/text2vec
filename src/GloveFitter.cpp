#include "GloveFit.h"

struct AdaGradIter : public Worker {
  RVector<int> x_irow;
  RVector<int> x_icol;
  RVector<double> x_val;
  RVector<int> iter_order;

  GloveFit &fit;

  int vocab_size;
  int word_vec_size;
  int num_iters;
  int x_max;
  double learning_rate;

  // accumulated value
  double global_cost;

  // function to set global_cost = 0 between iterations
  void set_cost_zero() { global_cost = 0; };

  //init function to use with rcpp-modules
  void init(const IntegerVector &x_irowR,
            const IntegerVector &x_icolR,
            const NumericVector &x_valR,
            const IntegerVector &iter_orderR,
            GloveFit &fit) {
    x_irow = RVector<int>(x_irowR);
    x_icol = RVector<int>(x_icolR);
    x_val  = RVector<double> (x_valR);
    iter_order = RVector<int> (iter_orderR);
    fit = fit;
    global_cost = 0;
  }


  // dummy constructor
  // used in first initialization of GloveFitter
  AdaGradIter(GloveFit &fit):
    x_irow(IntegerVector(0)),
    x_icol(IntegerVector(0)),
    x_val(NumericVector(0)),
    iter_order(IntegerVector(0)),
    fit(fit) {};

  // constructors
  AdaGradIter(const IntegerVector &x_irowR,
              const IntegerVector &x_icolR,
              const NumericVector &x_valR,
              const IntegerVector &iter_orderR,
              GloveFit &fit):
    x_irow(x_irowR),
    x_icol(x_icolR),
    x_val(x_valR),
    iter_order(iter_orderR),
    fit(fit),
    global_cost(0) {}

  AdaGradIter(const AdaGradIter& AdaGradIter, Split):
    x_irow(AdaGradIter.x_irow),
    x_icol(AdaGradIter.x_icol),
    x_val(AdaGradIter.x_val),
    iter_order(AdaGradIter.iter_order),
    fit(AdaGradIter.fit),
    global_cost(0) {}

  // process just the elements of the range
  void operator()(size_t begin, size_t end) {
    global_cost += this->fit.partial_fit(begin, end, x_irow, x_icol, x_val, iter_order);
  }
  // join my value with that of another global_cost
  void join(const AdaGradIter& rhs) {
    global_cost += rhs.global_cost;
  }
};

class GloveFitter {
public:
  //------------------------------------------------
  GloveFitter(List params):
    GRAIN_SIZE(as<uint32_t>(params["grain_size"])),
    gloveFit(as<size_t>(params["vocab_size"]),
             as<size_t>(params["word_vec_size"]),
             as<float>(params["learning_rate"]),
             as<uint32_t>(params["x_max"]),
             as<float>(params["max_cost"]),
             as<float>(params["alpha"]),
             as<float>(params["lambda"])),
    adaGradIter(gloveFit) {}
  //------------------------------------------------
  void set_cost_zero() {
    adaGradIter.set_cost_zero();
  }
  float get_word_vectors_sparsity_ratio() {
    return adaGradIter.fit.get_word_vectors_sparsity_ratio();
  }
  double partial_fit(const IntegerVector x_irow,
                   const IntegerVector  x_icol,
                   const NumericVector x_val,
                   const IntegerVector iter_order) {
    this->adaGradIter.init(x_irow, x_icol, x_val, iter_order, gloveFit);
    parallelReduce(0, x_irow.size(), adaGradIter, GRAIN_SIZE);
    return (this->adaGradIter.global_cost);
  }

  NumericMatrix get_word_vectors() {
  // List get_word_vectors() {
    return adaGradIter.fit.get_word_vectors();
  }

private:
  uint32_t GRAIN_SIZE;
  GloveFit gloveFit;
  AdaGradIter adaGradIter;
};

RCPP_MODULE(GloveFitter) {
  class_< GloveFitter >( "GloveFitter" )
  .constructor<List>()
  .method( "get_word_vectors", &GloveFitter::get_word_vectors, "returns word vectors")
  .method( "set_cost_zero", &GloveFitter::set_cost_zero, "sets cost to zero")
  .method( "partial_fit", &GloveFitter::partial_fit, "process TCM data chunk")
  .method( "get_sparsity_level", &GloveFitter::get_word_vectors_sparsity_ratio, "return current sparsity level")
  ;
}
