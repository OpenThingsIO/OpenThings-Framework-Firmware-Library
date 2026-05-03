#ifndef OTF_LINKEDMAP_H
#define OTF_LINKEDMAP_H

#if defined(ARDUINO)
#include <Arduino.h>
#else
#include <string.h>
#endif

namespace OTF {
  template<class T>
  class LinkedMapNode;

  template<class T>
  class LinkedMap {
    friend class OpenThingsFramework;

    friend class Response;

  private:
    LinkedMapNode<T> *head = nullptr;
    LinkedMapNode<T> *tail = nullptr;

    LinkedMapNode<T> *_findNode(const char *key, bool keyInFlash = false) const {
      LinkedMapNode<T> *node = head;
      while (node != nullptr) {
        #if defined(ARDUINO)
        if ((keyInFlash ? strcmp_P(node->key, key) : strcmp(node->key, key)) == 0) {
        #else
        if (strcmp(node->key, key) == 0) {
        #endif
          return node;
        }

        node = node->next;
      }

      // Indicate the key could not be found.
      return nullptr;
    }

    void _add(LinkedMapNode<T> *node, bool keyInFlash = false) {
      LinkedMapNode<T> *existingNode = _findNode(node->key, keyInFlash);
      if (existingNode != nullptr) {
        // Update the value of the existing node.
        existingNode->value = node->value;
        delete node;
      } else {
        // Add the new node to the end of the list.
        if (head == nullptr) {
          head = node;
          tail = head;
        } else {
          tail->next = node;
          tail = tail->next;
        }
      }
    }

    T _find(const char *key, bool keyInFlash = false) const {
      LinkedMapNode<T> *node = _findNode(key, keyInFlash);
      return node != nullptr ? node->value : nullptr;
    }

  public:
    ~LinkedMap() {
      LinkedMapNode<T> *node = head;
      while (node != nullptr) {
        LinkedMapNode<T> *next = node->next;
        delete node;
        node = next;
      }
    }

    // Borrowed: caller owns the key buffer; node does not free it.
    void add(const char *key, T value) {
      _add(new LinkedMapNode<T>(key, value));
    }

    // Owned: node takes ownership of a heap-allocated `new char[]` key
    // and frees it on destruction. Non-const parameter signals ownership
    // transfer and rejects string literals at the call site.
    void addOwned(char *key, T value) {
      _add(new LinkedMapNode<T>(key, value, true));
    }

    #if defined(ARDUINO)
    void add(const __FlashStringHelper *key, T value) {
      _add(new LinkedMapNode<T>(key, value), true);
    }
    #endif
    
    T find(const char *key) const {
      return _find(key, false);
    }

    #if defined(ARDUINO)
    T find(const __FlashStringHelper *key) const {
      return _find((char *) key, true);
    }
    #endif    
  };

  template<class T>
  class LinkedMapNode {
  private:
    /** True if the node owns its key buffer (allocated via `new char[]`)
     * and must free it on destruction. False for borrowed pointers
     * (e.g., keys pointing into a parsed request buffer). */
    bool ownsKey = false;

  public:
    const char *key = nullptr;
    T value;
    LinkedMapNode<T> *next = nullptr;

    LinkedMapNode(const char *key, T value) {
      this->key = key;
      this->value = value;
    }

    LinkedMapNode(const char *key, T value, bool ownsKey) {
      this->key = key;
      this->value = value;
      this->ownsKey = ownsKey;
    }

    ~LinkedMapNode() {
      if (ownsKey) {
        delete[] key;
      }
    }
  };
}// namespace OTF

#endif
