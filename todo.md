### Todo
    - (*) Break out user input into separate functions
    - Features: 
        - ( ) Add categories
        - (*) Ability to edit a specific field
        - ( ) Command line arguments?
    - (*) Get rid of strtok and use strsep
    - ( ) Create some unit tests for input validation
    - (*) Sorting The CSV By Date
    - ( ) In the read function, implement a way to list what dates have data
            in them.
    - ( ) In the read function, implement a better warning for accessing a 
            date range that doesn't contain any data.

#### Main Feature Set
    - (*) Create a transaction
    - (*) View     " "
    - (*) Update   " "
    - (*) Delete   " "

#### Bugs
    - (*) A line number is able to be selected (line 1) and it will edit the
            header line. Also line 1 is displayed as line 2. Probably something
            to do with the humanreadable target variable being subtracted by 2
