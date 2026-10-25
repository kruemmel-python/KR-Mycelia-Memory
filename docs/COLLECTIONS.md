# Collections

Collections are DAD records in the `memory_collections` table. Memberships are
separate `memory_relationships` records (`belongs_to`), so a memory may belong
to more than one collection.

The native core supports create, rename, archive, merge and delete. A merge
moves memberships to the target collection and archives the source; it does
not delete either member memories. Deleting a collection removes only its
membership links and the collection record, never the linked memories.

File imports automatically create or reuse a source-folder collection and
record their membership. MCP clients with `write` may create, rename and merge
collections; `delete` is required for permanent collection deletion.

Automatic semantic grouping and the desktop collection-management view are
not complete yet. The existing automatic source-folder assignment is an
evidence-preserving grouping rule, not a claim of semantic classification.
