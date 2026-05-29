import caast
import sys

print("==============================================")
print("--- A-AST Cython Bridge Verbose Inspection ---")
print("==============================================\n")

print("1. Instantiation (File I/O across C-Boundary)")
print("Loading '../utf8_nfc.aast'...")
tree = caast.load_from_file("../utf8_nfc.aast")

# Prove this is a compiled C-extension object, not a normal Python class
print(f"   Returned Object Type: {type(tree)}")
print(f"   Has Python __dict__?  {hasattr(tree, '__dict__')} (Should be False for raw Cython objects)\n")

print("2. Property Extraction (C-Pointer Dereferencing)")
print(f"   Node.type:        '{tree.type}'")
print(f"   Node.hash:        '{tree.hash}'")
print(f"   Node.child_count: {tree.child_count}")
print(f"   Node.payload:     {tree.payload}\n")

print("3. Eager Iteration (C-Callback executing across the GIL)")
children = tree.get_children()
print(f"   Total children retrieved: {len(children)}")
if len(children) > 0:
    first_key, first_node = children[0]
    print(f"   First Child Key:   '{first_key}'")
    print(f"   First Child Type:  {type(first_node)}")
    print(f"   First Child Hash:  '{first_node.hash}'\n")

print("4. Deep Path Query (O1 Pointer Resolution)")
print("   Querying path: ['80', '80'] ...")
deep_node = tree.query_path(["80", "80"])
if deep_node is not None:
    print(f"   Deep Node Found!  Type: {type(deep_node)}")
    print(f"   Deep Node.type:   '{deep_node.type}'")
    print(f"   Deep Node.hash:   '{deep_node.hash}'")
else:
    print("   Deep Node not found.\n")

print("\n5. Boundary Enforcement (Immutability Check)")
print("   Attempting illegal write: tree.hash = 'BOGUS_HASH'")
try:
    tree.hash = "BOGUS_HASH"
    print("   [!] CRITICAL FAILURE: Bridge allowed mutation!")
    sys.exit(1)
except Exception as e:
    print(f"   [SUCCESS] Mutation violently rejected.")
    print(f"   [SUCCESS] Exception Type:    {type(e)}")
    print(f"   [SUCCESS] Exception Message: '{e}'")

print("\n==============================================")
print("--- Inspection Complete ---")
