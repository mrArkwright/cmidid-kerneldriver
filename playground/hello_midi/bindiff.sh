cmp -l hello_midi.ko hello_midi.invalidformat.ko | 
mawk 'function oct2dec(oct,     dec) {
          for (i = 1; i <= length(oct); i++) {
              dec *= 8;
              dec += substr(oct, i, 1)
          };
          return dec
      }
      {
          printf "%08X %02X %02X\n", $1, oct2dec($2), oct2dec($3)
      }'
