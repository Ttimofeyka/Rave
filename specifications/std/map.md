# std/map

Hash map implementation with generic key-value pairs.

## Types

### std::hashmap<K, V>

Generic hash map structure that stores key-value pairs using hash-based indexing.

**Conditions:** K and V must not be void types.

**Fields:**
- `entry<K, V>** table` - Internal hash table
- `usize length` - Number of entries in the map
- `usize mapSize` - Size of the hash table

### std::hashmap::entry<KE, VE>

Internal entry structure for hash map buckets.

**Fields:**
- `KE key` - Entry key
- `VE value` - Entry value
- `entry<KE, VE>* next` - Next entry in collision chain

### std::hashmap::iterator<KI, VI>

Iterator for traversing hash map entries.

**Fields:**
- `usize currentHighest` - Current bucket index
- `entry<KI, VI>* current` - Current entry pointer

## Functions

### Constructor

```d
std::hashmap<K, V> this()
std::hashmap<K, V> this(usize mapSize)
```

Creates a new hash map with default size (128) or specified size.

### set

```d
void set(K key, V value)
```

Inserts or updates a key-value pair. Automatically resizes when load factor exceeds 0.75.

### find

```d
std::hashmap::entry<K, V>* find(K key)
```

Returns pointer to entry with given key, or null if not found.

### contains

```d
bool contains(K key)
```

Returns true if key exists in the map.

### get

```d
V get(K key)
```

Returns value for given key, or null if not found.

### remove

```d
bool remove(K key)
```

Removes entry with given key. Returns true if removed, false if not found.

### clear

```d
void clear()
```

Removes all entries from the map.

### next

```d
void next(std::hashmap::iterator<K, V>* iterator)
```

Advances iterator to next entry in the map.

## Example

```d
import <std/map>

std::hashmap<int, int> map = std::hashmap<int, int>();
map.set(1, 100);
map.set(2, 200);

if (map.contains(1)) {
    int value = map.get(1);
}

map.remove(2);
map.clear();
```
