# Grammar

The grammar of the Rave programming language is similar to the grammar of the C, C++ and D programming languages.

To check this, it is enough to make one example:
```d
int a = 0;
int b = 0;

while(a == b) {a += 1;}
if(a == 1) b = 1;
```

This code will work the same on Rave, C, C++ and D, which gives Rave some compatibility with other languages.
