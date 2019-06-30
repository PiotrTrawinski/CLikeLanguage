# CLikeLanguage
Compiler for my own programming language

The language itself is designed to be like C++ in that it provides low-level capabilities with higher level features for convinience, but have cleaner syntax + some additional features not included in C++ (So in a sense it's probably more similar to D then C++).

The compiler takes as an input paths to directories and files containing code written in created language and goes 4 through stages:
- Parsing

  All provided files aswell as all file added through "#include" directive are read and saved as tokens (tokens such as "label", "integer", "string literal" etc)

- Code tree creating

  Things like code blocks, keywords, declarations, expressions are identified and saved in a hierarchical tree structure
    
- Interpreting

  All of the statements, operations, etc are interpreted so that after this stage the whole code is understood and ready to be translated to another language

- LLVM code creating

  Creating LLVM code through provided C++ API. interpreted code tree is translated to LLVM code tree which then can be used to optimize and output machine code/executable
  
Example code:

- quicksort + partition (template):
  ```C++ 
  quicksort<T>(tab [*]T) {
      if tab.size > 1 {
          p := partition(tab);
          quicksort(tab[0 : p-1]);
          quicksort(tab[p+1 : tab.size-1]);
      }

      partition(tab [*]T) -> int {
          pivot := tab[tab.size-1];
          p := 0;
          for tab {
              if (it < pivot) {
                  swap(it, tab[p]);
                  p += 1;
              }
          }
         swap(tab[p], tab[tab.size-1]);
         return p;
      }
  }  
  ```

  Althrough the above code, in my opinion, looks quite clean, it's not any slower then C++.
  The execution time compared to C++ version using std::vector + start/end indexes compiled with g++/clang/visual C++ is actually faster.

  It is however a cheat, because it uses build-in "[*]T" type (array-view type) which is different from std::vector + indexes. In an equal test where you either implement the same structure in C++ or use iterators (which are less readable) the times are similar.
  
  The point here is that this language can be as fast as C++ and in some cases it is easier to write optimal code in it compared to C++.

- few unusual features:
  ```C++ 
    #include std

    main() -> int {

        // casting is used to interpret the bits differently (it is guaranteed to be no-op)
        x := 3.7;
        outputln(i16(x)); // output: "3"        (float to 16-bit integer conversion)
        outputln(<i16>(x)); // output: "-26214" (float to 16-bit integer casting)

        tab := ['a', 'b', 'c', 'd']; // creating static array o type [4]u8 (u8 = 8 bit unsiged)
        y := <i32>(tab); // creating 32-bit integer by casting from array (if little-endian then high byte is 'd' and low byte is 'a')


        // -- error/onerror/onsuccess
        divide(a int, b int) -> ?int { // function returning division result or error (?int is simlar to C++ std::optional<int>)
            if b == 0 then return error; // could be also for example error(-2) to specify error code
            else return a / b;
        }

        a := divide(1, 2); // a is of type ?int
        b := divide(3, 4) onerror return 1; // b is of type int. if the divide fails then "return 1" is executed

        c := divide(5, 6) onerror { // c is of type int. if divide fails then onerror block is executed
            a = b = c = 0;
        } onsuccess { // if divide does not fail then onsuccess block is executed.
           b *= c; 
        }


        // for loop syntax
        elements := [1, 2, 3];

        for e := elements {} // iterating over elements by value (e is of type int)
        for e &= elements {} // by reference       (e is of type &int)
        for e :: elements {} // by const value     (e is of type const int)

        for e, i := elements { // iterate over elements (e) and their indexes (i) 
            outputln("element ", e, " has index ", i);
        }

        for elements { // shorthand for "for it, index &= elements"
            it = index*2; // every element is equal to its index*2 so elements = [0, 2, 4] 
        }


        // break/continue from nested loops
        for i := [1, 2, 3] {
            output(i);
            for j := [4, 5, 6] {
                output(j);
                if j == 5 then break i;
            }
        } // output: "145"


        // "long-circuit" logical OR and AND
        if x == 5 &&  divide(1,2) {} // function divide(1,2) will only be executed if x == 5
        if x == 5 &&& divide(1,2) {} // function divide(1,2) will always be executed


        // build-in std::unique_ptr like type (additionally to raw pointers):
        ptr := alloc int(6); // ptr is of type "^int". C++: auto ptr = std::make_unique<int>(6);
        val := $ptr; // val is of type "int". C++: int val = *ptr;

        raw := @val; // raw is of type "*int" (raw pointer)
        val = $raw; // val is of type "int"
        // also note that instead of reusing '&' and '*' symbols here you use '@' to get address and '$' to get value from address

        return 0;
    }
  ```
