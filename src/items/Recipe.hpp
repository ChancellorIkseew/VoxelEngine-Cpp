#pragma once

#include <vector>

#include "typedefs.hpp"

enum class ItemsOrder {
    FREE,      // order of items is not important
    POSITIONAL // order of items is important
};

class Recipe {
    std::vector<itemid_t> ingr;
    ItemsOrder order;
    itemid_t product;
public:
    Recipe() = default;
    ~Recipe() = default;
    Recipe(itemid_t product, ItemsOrder order, std::vector<itemid_t> ingr);

    itemid_t getProduct() const {
        return product;
    }
};