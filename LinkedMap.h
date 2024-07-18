#ifndef OTF_LINKEDMAP_H
#define OTF_LINKEDMAP_H

#define KEY_MAX_LENGTH 100
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

    void add(const char *key, T value) {
      _add(new LinkedMapNode<T>(key, value));
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
    /** Indicates if the key was copied into RAM from flash memory and needs to be freed when the object is destroyed. */
    bool keyFromFlash = false;

  public:
    const char *key = nullptr;
    T value;
    LinkedMapNode<T> *next = nullptr;

    // LinkedMapNode(const __FlashStringHelper *key, T value) {
    //   keyFromFlash = true;
    //   char *_key = new char[KEY_MAX_LENGTH];
    //   strncpy_P(_key, (char *) key, KEY_MAX_LENGTH);
    //   this->key = (const char *) _key;

    //   this->value = value;
    // }

    LinkedMapNode(const char *key, T value) {
      this->key = key;
      this->value = value;
    }

    ~LinkedMapNode() {
      // Delete the key if it was copied into RAM from flash memory.
      if (keyFromFlash) {
        delete key;
      }
    }
  };
}// namespace OTF

#endif
