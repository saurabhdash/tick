
// License: BSD 3 clause

#include "online_forest_classifier.h"

/*********************************************************************************
 * NodeClassifier methods
 *********************************************************************************/

NodeClassifier::NodeClassifier(TreeClassifier &tree, ulong parent)
    : _tree(tree) {
  _parent = parent;
  _left = 0;
  _right = 0;
  _n_samples = 0;
  _is_leaf = true;
  _weight = 0;
  _weight_tree = 0;
  _counts = ArrayULong(n_classes());
  _counts.fill(0);
}

NodeClassifier::NodeClassifier(const NodeClassifier &node)
    : _tree(node._tree),
      _parent(node._parent), _left(node._left), _right(node._right),
      _feature(node._feature), _threshold(node._threshold),
      _n_samples(node._n_samples),
      _x_t(node._x_t),
      _y_t(node._y_t),
      _weight(node._weight), _weight_tree(node._weight_tree),
      _is_leaf(node._is_leaf),
      _counts(node._counts) {}

NodeClassifier::NodeClassifier(const NodeClassifier &&node) : _tree(_tree) {
  _parent = node._parent;
  _left = node._left;
  _right = node._right;
  _feature = node._feature;
  _threshold = node._threshold;
  _n_samples = node._n_samples;
  _x_t = node._x_t;
  _y_t = node._y_t;
  _weight = node._weight;
  _weight_tree = node._weight_tree;
  _is_leaf = node._is_leaf;
  _counts = node._counts;
}

void NodeClassifier::update_downwards(const ArrayDouble &x_t, const double y_t) {
  _n_samples++;
  _weight -= step() * loss(y_t);
  update_predict(y_t);
}

void NodeClassifier::update_upwards() {
  if (_is_leaf) {
    _weight_tree = _weight;
  } else {
    _weight_tree = log_sum_2_exp(_weight, node(_left).weight_tree() + node(_right).weight_tree());
  }
}

void NodeClassifier::update_predict(const double y_t) {
  // We update the counts for the class y_t
  _counts[static_cast<uint8_t>(y_t)]++;
}

double NodeClassifier::score(uint8_t c) const {
  // Using Dirichet(1/2, ... 1/2) prior
  return static_cast<double>(2 * _counts[c] + 1) / (2 * _n_samples + n_classes());
}

inline void NodeClassifier::predict(ArrayDouble& scores) const {
  for (uint8_t c=0; c < n_classes(); ++c) {
    scores[c] = score(c);
  }
}

double NodeClassifier::loss(const double y_t) {
  // Log-loss
  uint8_t c = static_cast<uint8_t>(y_t);
  return -std::log(score(c));
}

inline NodeClassifier &NodeClassifier::node(ulong index) const {
  return _tree.node(index);
}

ulong NodeClassifier::n_features() const {
  return _tree.n_features();
}

uint8_t  NodeClassifier::n_classes() const {
  return _tree.n_classes();
}

inline double NodeClassifier::step() const {
  return _tree.step();
}

inline ulong NodeClassifier::parent() const {
  return _parent;
}

inline ulong NodeClassifier::left() const {
  return _left;
}

inline NodeClassifier &NodeClassifier::set_left(ulong left) {
  _left = left;
  return *this;
}

inline ulong NodeClassifier::right() const {
  return _right;
}

inline NodeClassifier &NodeClassifier::set_right(ulong right) {
  _right = right;
  return *this;
}

inline bool NodeClassifier::is_leaf() const {
  return _is_leaf;
}

inline NodeClassifier &NodeClassifier::set_is_leaf(bool is_leaf) {
  _is_leaf = is_leaf;
  return *this;
}

inline ulong NodeClassifier::feature() const {
  return _feature;
}

inline NodeClassifier &NodeClassifier::set_feature(ulong feature) {
  _feature = feature;
  return *this;
}

inline double NodeClassifier::threshold() const {
  return _threshold;
}

inline NodeClassifier &NodeClassifier::set_threshold(double threshold) {
  _threshold = threshold;
  return *this;
}

inline ulong NodeClassifier::n_samples() const {
  return _n_samples;
}

inline NodeClassifier &NodeClassifier::set_n_samples(ulong n_samples) {
  _n_samples = n_samples;
  return *this;
}

inline double NodeClassifier::weight() const {
  return _weight;
}

inline NodeClassifier &NodeClassifier::set_weight(double weight) {
  _weight = weight;
  return *this;
}

inline double NodeClassifier::weight_tree() const {
  return _weight_tree;
}

inline NodeClassifier &NodeClassifier::set_weight_tree(double weight_tree) {
  _weight_tree = weight_tree;
  return *this;
}

inline const ArrayDouble &NodeClassifier::x_t() const {
  return _x_t;
}

inline NodeClassifier &NodeClassifier::set_x_t(const ArrayDouble &x_t) {
  _x_t = x_t;
  return *this;
}

inline double NodeClassifier::y_t() const {
  return _y_t;
}

inline NodeClassifier &NodeClassifier::set_y_t(const double y_t) {
  _y_t = y_t;
  return *this;
}

void NodeClassifier::print() {
  std::cout << "Node(parent: " << _parent
      << ", left: " << _left
      << ", right: " << _right
      << ", n_samples: " << _n_samples
      << ", is_leaf: " << _is_leaf
      << ", feature: " << _feature
      << ", thresh: " << _threshold
      << ", scores: [" << std::setprecision(2) << score(0) << ", " << std::setprecision(2) << score(1) << "]"
      << ", counts: [" << std::setprecision(2) << _counts[0] << ", " << std::setprecision(2) << _counts[1] << "]"
      << ", weight: " << _weight
      << ", weight_tree: " << _weight_tree
      << ")\n";
}

/*********************************************************************************
* TreeClassifier methods
*********************************************************************************/

TreeClassifier::TreeClassifier(const TreeClassifier &tree)
    : forest(tree.forest), nodes(tree.nodes) {}

TreeClassifier::TreeClassifier(const TreeClassifier &&tree)
    : forest(tree.forest), nodes(tree.nodes) {}

TreeClassifier::TreeClassifier(OnlineForestClassifier &forest) : forest(forest) {
  // TODO: pre-allocate the vector to make things faster ?
  add_node(0);
}

ulong TreeClassifier::split_leaf(ulong index, const ArrayDouble &x_t, double y_t) {
  // std::cout << "Splitting node " << index << std::endl;
  ulong left = add_node(index);
  ulong right = add_node(index);
  node(index).set_left(left).set_right(right).set_is_leaf(false);

  // TODO: better feature sampling
  ulong feature = forest.sample_feature();

  double x1_tj = x_t[feature];
  double x2_tj = node(index).x_t()[feature];
  double threshold;

  // The leaf that contains the passed sample (x_t, y_t)
  ulong data_leaf;
  ulong other_leaf;

  // std::cout << "x1_tj= " << x1_tj << " x2_tj= " << x2_tj << " threshold= " << threshold << std::endl;
  // TODO: what if x1_tj == x2_tj. Must be taken care of by sample_feature()
  if (x1_tj < x2_tj) {
    threshold = forest.sample_threshold(x1_tj, x2_tj);
    data_leaf = left;
    other_leaf = right;
  } else {
    threshold = forest.sample_threshold(x2_tj, x1_tj);
    data_leaf = right;
    other_leaf = left;
  }
  // TODO: code a move_sample
  NodeClassifier & current_node = node(index);
  NodeClassifier & data_node = node(data_leaf);
  NodeClassifier & other_node = node(other_leaf);
  current_node.set_feature(feature).set_threshold(threshold);
  // We pass the sample to the new leaves, and initialize the _label_average with the value
  data_node.set_x_t(x_t).set_y_t(y_t);

  // other_node.set_x_t(current_node.x_t()).set_y_t(current_node.y_t());
  other_node.set_x_t(current_node.x_t()).set_y_t(current_node.y_t());

  // Update downwards of v'
  other_node.update_downwards(current_node.x_t(), current_node.y_t());
  // Update upwards of v': it's a leaf
  other_node.update_upwards();
  // node(other_leaf).set_weight_tree(node(other_leaf).weight());
  // Update downwards of v''
  data_node.update_downwards(x_t, y_t);
  // Note: the update_up of v'' is done in the go_up method, called in fit()
  // std::cout << "Done splitting node." << std::endl;
  return data_leaf;
}

ulong TreeClassifier::go_downwards(const ArrayDouble &x_t, double y_t, bool predict) {
  // Find the leaf that contains the sample
  // Start at the root. Index of the root is always 0
  // If predict == true, this call to find_leaf is for
  // prediction only, so that no leaf update and splits can be done
  // std::cout << "Going downwards" << std::endl;
  ulong index_current_node = 0;
  bool is_leaf = false;
  while (!is_leaf) {
    // Get the current node
    NodeClassifier &current_node = node(index_current_node);
    if (!predict) {
      current_node.update_downwards(x_t, y_t);
    }
    // Is the node a leaf ?
    is_leaf = current_node.is_leaf();
    if (!is_leaf) {
      if (x_t[current_node.feature()] <= current_node.threshold()) {
        index_current_node = current_node.left();
      } else {
        index_current_node = current_node.right();
      }
    }
  }
  // std::cout << "Done going downwards" << std::endl;
  return index_current_node;
}

void TreeClassifier::go_upwards(ulong leaf_index) {
  // std::cout << "Going upwards" << std::endl;
  ulong current = leaf_index;
  while (true) {
    NodeClassifier &current_node = node(current);
    current_node.update_upwards();
    if (current == 0) {
      // std::cout << "Done going upwards" << std::endl;
      break;
    }
    // We must update the root node
    current = node(current).parent();
  }
}

inline ulong TreeClassifier::n_nodes() const {
  return _n_nodes;
}

void TreeClassifier::fit(const ArrayDouble &x_t, double y_t) {
  // TODO: Test that the size does not change within successive calls to fit
//  std::cout << "iteration: " << iteration << std::endl;
//  print();
  if (iteration == 0) {
    nodes[0].set_x_t(x_t).set_y_t(y_t);
    iteration++;
    return;
  }
  ulong leaf = go_downwards(x_t, y_t, false);
  ulong new_leaf = split_leaf(leaf, x_t, y_t);
//  for(ulong j=0; j < n_features(); ++j) {
//    double delta = std::abs(x_t[j] - node(leaf).sample().first[j]);
//    if (delta > 0.) {
//      new_leaf = split_node(leaf, x_t, y_t);
//      break;
//    }
//  }
  go_upwards(new_leaf);
  iteration++;
}

void TreeClassifier::predict(const ArrayDouble &x_t, ArrayDouble& scores) {
  // std::cout << "Going downwards" << std::endl;
  ulong leaf = go_downwards(x_t, 0., true);
  // std::cout << "Done." << std::endl;
  ulong current = leaf;
  // The child of the current node that does not contain the data
  ArrayDouble pred_new(n_classes());
  while (true) {
    // std::cout << "node: " << current << std::endl;
    NodeClassifier &current_node = node(current);
    if (current_node.is_leaf()) {
      current_node.predict(scores);
    } else {
      double w = std::exp(current_node.weight() - current_node.weight_tree());
      // Get the predictions of the current node
      current_node.predict(pred_new);
      for(uint8_t c = 0; c < n_classes(); ++c) {
        scores[c] = 0.5 * w * pred_new[c] + (1 - 0.5 * w) * scores[c];
      }
    }
    // Root must be updated as well
    if (current == 0) {
      break;
    }
    current = current_node.parent();
  }
}

ulong TreeClassifier::add_node(ulong parent) {
  nodes.emplace_back(*this, parent);
  return _n_nodes++;
}

inline ulong TreeClassifier::n_features() const {
  return forest.n_features();
}

inline uint8_t TreeClassifier::n_classes() const {
  return forest.n_classes();
}

inline double TreeClassifier::step() const {
  return forest.step();
}

inline CriterionClassifier TreeClassifier::criterion() const {
  return forest.criterion();
}

/*********************************************************************************
 * OnlineForestClassifier methods
 *********************************************************************************/

OnlineForestClassifier::OnlineForestClassifier(uint32_t n_trees,
                                               uint8_t n_classes,
                                               double step,
                                               CriterionClassifier criterion,
                                               int32_t n_threads,
                                               int seed,
                                               bool verbose)
    : _n_trees(n_trees), _n_classes(n_classes), _n_threads(n_threads),
      _criterion(criterion), _step(step), _verbose(verbose), trees() {
  // No iteration so far
  _iteration = 0;
  create_trees();
  // Seed the random number generators
  set_seed(seed);
}

OnlineForestClassifier::~OnlineForestClassifier() {}

void OnlineForestClassifier::create_trees() {
  // Just in case...
  trees.clear();
  trees.reserve(_n_trees);
  for (uint32_t i = 0; i < _n_trees; ++i) {
    trees.emplace_back(*this);
  }
}

void OnlineForestClassifier::fit(const SArrayDouble2dPtr features,
                                const SArrayDoublePtr labels) {
  // std::cout << "OnlineForestClassifier::fit" << std::endl;
  ulong n_samples = features->n_rows();
  ulong n_features = features->n_cols();
  set_n_features(n_features);
  for (ulong i = 0; i < n_samples; ++i) {
    for (TreeClassifier &tree : trees) {
      // Fit the tree online using the new data point
      tree.fit(view_row(*features, i), (*labels)[i]);
    }
    _iteration++;
  }
  // std::cout << "Done OnlineForestClassifier::fit" << std::endl;
}

void OnlineForestClassifier::predict(const SArrayDouble2dPtr features,
                                     SArrayDouble2dPtr predictions,
                                     bool use_aggregation) {
  predictions->fill(0.);
  if (_iteration > 0) {
    ulong n_samples = features->n_rows();
    ArrayDouble scores_tree(_n_classes);
    scores_tree.fill(0.);
    ArrayDouble scores_forest(_n_classes);
    scores_forest.fill(0.);
    for (ulong i = 0; i < n_samples; ++i) {
      // The prediction is simply the average of the predictions
      ArrayDouble scores_i = view_row(*predictions, i);
      for (TreeClassifier &tree : trees) {
        tree.predict(view_row(*features, i), scores_tree);
        // TODO: use a .incr method instead ??
        scores_i.mult_incr(scores_tree, 1.);
      }
      scores_i /= _n_trees;
    }
  } else {
  TICK_ERROR("You must call ``fit`` before ``predict``.")
}
}

void OnlineForestClassifier::clear() {
  create_trees();
  _iteration = 0;
}

inline ulong OnlineForestClassifier::sample_feature() {
  return rand.uniform_int(0L, n_features() - 1);
}

inline double OnlineForestClassifier::sample_threshold(double left, double right) {
  return rand.uniform(left, right);
}

//inline bool OnlineForestClassifier::verbose() const {
//  return _verbose;
//}
//
//inline OnlineForestClassifier &OnlineForestClassifier::set_verbose(bool verbose) {
//  _verbose = verbose;
//  return *this;
//}