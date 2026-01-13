### Todo
    - (*) Break out user input into separate functions
    - Features: 
        - ( ) Categories
            - ( ) View transactions based on category
            - (*) Add categories
            - ( ) Delete categories
            - ( ) Allocate monthly budget by category
            - (*) List Categories
        - (*) Ability to edit a specific field
        - (*) Command line arguments?
    - (*) Get rid of strtok and use strsep
    - ( ) Create some unit tests for input validation
    - (*) Sorting The CSV By Date
    - (*) In the read function, implement a way to list what dates have data
            in them.
    - (*) In the read function, implement a better warning for accessing a 
            date range that doesn't contain any data.
    - (*) Display transaction type as a string instead of a bare number
    - (*) Show selected month and year at the top of the screen after
            selection on read
    - (*) Remove all reallocarray() functions for portability
    - ( ) Suggested to me that the user should be able to see all months,
            regardless if there's a record in that date range or not. Maybe
            throw an asterisk on the month if it contains records/vice versa
    - ( ) After main feature set is done and working, refactor to make main.c
            have a clearer control flow purpose
    - ( ) Implement visual truncation when there are not enough columns to
            display all the field data

#### Main Feature Set
    - (*) Create a transaction
    - (*) View     " "
    - (*) Update   " "
    - (*) Delete   " "
    - ( ) ncurses <-- The big one
        - (*) Welcome Menu
        - (*) Read
            - (*) Add ability to scroll through records if they are too
                    big to fit on the screen
            - (*) Columns for fields for easier reading
        - ( ) Edit
        - ( ) Confirm windows
        - ( ) Categories
            - ( ) When adding a transaction, select a category or add new
                    in an NCURSES menu.
        - ( ) Overview
        - (*) Scroll through dates
        - ( ) Resizing
    - (*) Basic Bar Graph Trends

#### Bugs
    - (*) A line number is able to be selected (line 1) and it will edit the
            header line. Also line 1 is displayed as line 2. Probably something
            to do with the humanreadable target variable being subtracted by 2
    - (*) Sorter places a previous year on line 3 when a more recent year is on
            line 2.
    - (*) Sorter edge case on adding a transaction to a month which does not
            match and is not greater than the target month. If the months in
            the CSV are all less than the target, the record will be 
            erroneously be added out of chronological order.
    - (*) Edge case where trying to access read via the TUI, fgets fails to
            read the line (because it doesn't exist).
