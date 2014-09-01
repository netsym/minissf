Optional Source-Code Annotations
--------------------------------

MiniSSF uses `LLVM <http://llvm.org/>`_ and `Clang <http://clang.llvm.org/>`_ for automatic C++ source-to-source translation to embed the handcrafted multithreading mechanism inside the user C++ code. The multithreading mechanism is need to support process-oriented simulation. In the absence of LLVM/Clang, the user can choose to manually add annotations in the source code so that MiniSSF source-code translator can parse them and embed the handcrafted multithreading mechanism accordingly. In this section, we describe the annotations required by the translation process. **If you use LLVM/Clang, you can safely skip this section. All restrictions mentioned in this section apply only to manual annotations without using LLVM/Clang.**

Why Source-to-Source Translation?
=================================

The source files of a MiniSSF model are *almost* pure C++: the syntactic correctness of the code can be checked by ordinary means, before and after substitutions are made in the text. C++ is a very complex language, the source-code translation is a delicate business. MiniSSF uses a perl script to parse and translate the source code. The restrictions we impose for the source code are relatively small.  To understand the source of these restrictions, it is helpful to understand what the preprocessor does.

As a MiniSSF process executes it may call other methods. At the point it suspends, any number of method calls may be piled up on the runtime stack. When the process resumes execution, it needs to be able to return to the exact location of the function (back through that call-chain) to where the process was suspended.  We call these methods procedures since it is possible for the methods to be on the runtime stack at the point of a process suspension.  Any method that calls a wait statement is a procedure.  Any method that calls a procedure is also a procedure.  The source-to-source translator instruments the bodies of procedures as well as the class declaration of classes that contain procedures.

The instrumentation of a class definition is minor.  The instrumentation of a procedure body is more complex.  To begin with, the local variables of a procedure body have to be dealt with specially, at least those variables whose values are expected to persist across process suspension. The source-to-source translator requires the user to identify such local variables syntactically.  It removes them as local variables altogether, creating the corresponding variables in a ``frame`` data structure that represents the the procedure call. All references to such variables in the body of the procedure are transformed into references to their representation in the frame.  Since input arguments are local variables that are expected to persist across process suspension, they too appear in the frame. The frame for a procedure must be created *before* the actual procedure call.  The definition, construction, and placement of a procedure's frame plays a large part of the main task of the source-to-source translator.  Additional complications arise if the procedure returns a value to its caller---space for an address for receiving the return value is also required in the frame.

To find the right place to instrument the source code, MiniSSF requires help from the modeler. We adopt a method similar to the High Performance Fortran, where special comments are used to annotate the source code in necessary places.  Likewise, the MiniSSF source-to-source translator looks for the annotations.  These special annotations are flagged by the sequence ``//! SSF``. At least one space between ! and SSF is expected.

State Variables
===============

Local variables in a procedure body that must persist across suspension have to be explicitly identified as state variables.  This is done using the ``//! SSF STATE`` annotation at the end of the line where the variable is declared inside the procedure. An example is shown below:: 

   int foo() {
     ltime_t x; //! SSF STATE 
     float y;   //! SSF STATE
     int z;     //! SSF STATE
     ...
   };

Several limitations exist:

* Only one variable may appear on each annotated line;
* An annotated variable may not be initialized in its declaration;
* The annotated variables must be declared at the beginning of the procedure body;
* The variables do not respect scope, that is, the scope of an annotated variable is the entire procedure body. Consider the wrong code below::

   int foo() {
      int x;
      ...
      {  int x;
         int y;
         ...
      }
    }

In C++ this would be perfectly legitimate code; for manual annotation in MiniSSF, it is impossible to declare both instances ``x`` to be procedure state variables. There is no distinction between the two instances of ``x``, since both of them will be visible in the procedure. The reason for this limitation is that the source-to-source translator just scans the beginning of a procedure body, makes a list of STATE variables, and creates a frame class that has a member variable for each identified procedure state variable.  A reference to ``x`` is transformed in the source to a reference to ``frame->x``.  It is within the realm of possibility to make the translator smart enough to understand nesting level and distinguish between commonly named variables at different scoping levels, but for manual annotation, at this point the benefit is outweighed by the effort required.  

A very important thing to note is that the source-to-source translator cannot detect whether a variable is used in a way that necessitates it being a state variable (e.g., its value is set before a call that potentially leads to process suspension, and that value is used again after the process resumes execution).  Failure to declare such a variable to be a state variable leads to mysterious bugs. It is recommended therefore all procedure local variables should be declared as state variables.

Procedures
==========

The ``//! SSF PROCEDURE`` annotation is used in two places.  The line containing the declaration of a method that will be designated as a procedure needs to be annotated at the end of the line where the method is declared inside the class declaration. In the following example, the ``update`` method is a procedure declared in the ``MyEntity`` class; the method should be declared like::

    class MyEntity : public Entity {
     pubic:
      ...
      void update(int); //! SSF PROCEDURE
      ...
    };


Elsewhere, presumably in the source file, the code body for the ``update`` method must be given. Here too the fact that ``update`` is a procedure must be flagged with an annotation. In this case the annotation needs to be placed on the line immediately prior to the declaration::

    //! SSF PROCEDURE
    void MyEntity::update(int x) {
      ...
    }

Another point, somewhat subtle, has to do with inheritance in C++. If method ``A::foo()`` is a virtual method and is a procedure, then any method ``B::foo()`` in a class ``B`` that is derived from ``A`` must also be a procedure.  Similarly, if any class ``C`` derived from ``A`` has method ``C::foo()`` that is a procedure, then ``A::foo()`` must too be declared as a procedure, as must ``B::foo()`` for any other class ``B`` that derives from ``A``.  The reason for this is that when ``A::foo()`` is called, the source-to-source translator cannot tell which of the derived classes is actually being called. If any one of them is a procedure, then they all must be so declared in order for the instrumentation to work properly.

Procedure Calls
===============

Instrumentation occurs where one procedure calls another.  In principle one would think that, since all procedures are identified, when a call to one of these procedures is found in code body of another procedure, it should be recognized.  That is true for a compiler, which has semantic information, but a mere preprocessor that handles the source code at the textual level is not that smart.  Our source-to-source translator can recognize that a word in the text corresponds to some procedure method name, but it cannot figure out the class instance. We require the modeler to tell the preprocessor when a procedure call is being made. The annotation for this is ``//! SSF CALL``. This is placed on the line *immediately* above the one where the call is made. A couple of examples are shown as follows::

   //! SSF PROCEDURE
   void foobar() {
     double x; //! SSF STATE
     ...

     //! SSF CALL
     join_server(x,y,z);

     //! SSF CALL
     x = getMin(x,a);
   }

The second case shows how to get a return value from a procedure.  For this to work ``x`` has to be declared earlier in the code body as a procedure state variable. It is unnecessary for ``a``, ``y``, and ``z`` to be state variables, at least not by virtue of being an argument to a procedure.  If the code expects their values to persist after the procedure call though, they need to be declared as state variables.  It is also required that a procedure call must occupy one and only one line. And it is generally a good idea to separate the procedure call from other expressions that involving the calculation of the arguments and the return value of this procedure call, for the sake of simplicity.

Limitations
===========

Instead of a full-fledged C++ parser, we use the simple preprocessor written in Perl to do the source-code translation. There are limitations on what one can reasonably expect what a preprocessor can accomplish. There are more limitations than what we could list here to reflect this fact.

* **Procedure bodies lie outside of class definition.** Every Procedure method must be declared on one line in its class definition, and have the code body described outside of the class definition. The source-to-source translator may not be able to detect a violation of this.

* **Name uniqueness.** Procedures must be distinguishable from each other by class name or function name. Other elements that constitutes a traditional C++ signature---return and argument types---do not play a role. For instance, one cannot in the same class use two procedures with the same name, one that accepts an integer argument and one that accepts a floating point argument.

* **Variable naming conventions.** Most internally declared variables have an ``_ssf_`` prefix.  One needs to avoid names with this prefix so that they will not conflict with any variables or methods used by the runtime system.

* **Comments.** Nested comments are not dealt with very well by the preprocessor. Avoid them.
