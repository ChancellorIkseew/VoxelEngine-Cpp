#include "Recipe.hpp"

Recipe::Recipe(itemid_t product, ItemsOrder order, std::vector<itemid_t> ingr)
    : product(product), order(order), ingr(std::move(ingr)) {
    int size = this->ingr.size();
    this->ingr.reserve(size);
}
