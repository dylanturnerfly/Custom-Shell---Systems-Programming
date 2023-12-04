Dylan Turner 207004235 dpt50


Test for Batch Mode:
--------------------
1. './mysh myscript.sh'
   -> Setup: Create a file myscript.sh and add whatever commands you want/
   -> Expected Result: Each command will be executed.


Test for Redirections:
---------------------
1. 'echo "Hello World" > output.txt'
   -> Expected Result: A file named output.txt is created with the content "Hello World"


2. 'cat < input.txt'
   -> Setup: Create a file called input.txt and add some content to it.
   -> Expected Result: The content of input.txt is displayed on the screen.

3. 'echo "New Line" > replace.txt'
   -> Setup: Create a file replace.txt.
   -> Expected Result: The "New Line" should be replace the content in replace.txt.

4. 'cat < input.txt > output.txt'
   -> Setup: Create a file input.txt with content.
   -> Expected Result: The content of input.txt is copied to output.txt.

5. 'nonexistentcommand > output.txt'
   -> Expected Result: An error message indicating the command is not found, and output.txt should be created but empty.

6. 'cat < empty.txt'
   -> Setup:  Create an empty file empty.txt.
   -> Expected Result: No output (as the file is empty).

7. 'cat < nonexisting.txt'
   -> Expected Result: An error message indicating the file doesn’t exist.

8. 'cat < input1.txt > output1.txt'
   ' cat < input2.txt > output2.txt'
   -> Setup: Create input1.txt and input2.txt with different contents
   -> Expected Result: output1.txt contains the content of input1.txt, and output2.txt contains the content of input2.txt.

9. 'echo "Test" >'
   -> Expected Result: An error message indicating invalid command syntax.
   
