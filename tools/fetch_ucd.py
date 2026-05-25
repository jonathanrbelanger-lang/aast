import urllib.request
import sys
import os

UCD_URL = "https://www.unicode.org/Public/UCD/latest/ucd/DerivedNormalizationProps.txt"
OUTPUT_FILE = "nfc_rules.txt"

def fetch_and_parse():
    print(f"Downloading UCD from {UCD_URL}...")
    try:
        response = urllib.request.urlopen(UCD_URL)
        text = response.read().decode('utf-8')
    except Exception as e:
        print(f"Failed to download: {e}")
        sys.exit(1)

    print("Parsing NFC_Quick_Check (NFC_QC) properties...")
    
    rules = []
    
    for line in text.split('\n'):
        # Strip comments and whitespace
        line = line.split('#')[0].strip()
        if not line:
            continue
            
        # We only care about lines containing NFC_QC
        if 'NFC_QC' not in line:
            continue
            
        # Parse the line: "0340..0341    ; NFC_QC; N"
        parts = [p.strip() for p in line.split(';')]
        
        # Double check it is exactly the NFC_QC property
        if len(parts) < 3 or parts[1] != 'NFC_QC':
            continue
            
        codepoint_range = parts[0]
        value = parts[2] # Will be 'N' (No) or 'M' (Maybe)
        
        # Handle ranges (XXXX..YYYY) or single codepoints (XXXX)
        if '..' in codepoint_range:
            start_hex, end_hex = codepoint_range.split('..')
            start = int(start_hex, 16)
            end = int(end_hex, 16)
        else:
            start = int(codepoint_range, 16)
            end = start
            
        # Convert each codepoint into UTF-8 raw bytes
        for cp in range(start, end + 1):
            try:
                # Convert to character, then encode to UTF-8
                utf8_bytes = chr(cp).encode('utf-8')
                # Format as hex strings (e.g., C3 80)
                hex_str = ' '.join(f"{b:02X}" for b in utf8_bytes)
                rules.append(f"{hex_str} : {value}")
            except Exception:
                # Skip surrogate halves which can't be encoded
                continue

    # Write the intermediate flat file
    with open(OUTPUT_FILE, 'w') as f:
        for rule in rules:
            f.write(f"{rule}\n")
            
    print(f"Successfully extracted {len(rules)} NFC_QC rules.")
    print(f"Saved intermediate file to: {OUTPUT_FILE}")

if __name__ == "__main__":
    fetch_and_parse()        if '..' in codepoint_range:
            start_hex, end_hex = codepoint_range.split('..')
            start = int(start_hex, 16)
            end = int(end_hex, 16)
        else:
            start = int(codepoint_range, 16)
            end = start
            
        # Convert each codepoint into UTF-8 raw bytes
        for cp in range(start, end + 1):
            try:
                # Convert to character, then encode to UTF-8
                utf8_bytes = chr(cp).encode('utf-8')
                # Format as hex strings (e.g., C3 80)
                hex_str = ' '.join(f"{b:02X}" for b in utf8_bytes)
                rules.append(f"{hex_str} : {value}")
            except Exception:
                # Skip surrogate halves which can't be encoded
                continue

    # Write the intermediate flat file
    with open(OUTPUT_FILE, 'w') as f:
        for rule in rules:
            f.write(f"{rule}\n")
            
    print(f"Successfully extracted {len(rules)} NFC_QC rules.")
    print(f"Saved intermediate file to: {OUTPUT_FILE}")

if __name__ == "__main__":
    fetch_and_parse()
