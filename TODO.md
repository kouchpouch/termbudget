### Todo
    - (*) Break out user input into separate functions
    - Features: 
        - ( ) Categories
            - ( ) View transactions based on category
            - (*) Add categories
            - (*) Delete categories NOTE: User must delete all transactions
                    assoc. with the category for it to be deleted.
            - ( ) Allocate monthly budget by category
            - (*) List Categories
        - (*) Ability to edit a specific field
        - (*) Command line arguments
            - (*) Debug
            - (*) CLI Mode
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
    - (*) Implement visual truncation when there are not enough columns to
            display all the field data
    - ( ) Move to integer/fixed point arithmetic for financial calculations
    - (*) *n/a* Use realloc in sorter.c (sort_csv()) instead of having a fixed
            number of lines to index
    - ( ) Add ability to escape the menu when accepting a user input
    - (*) Add page up/page down/end/home keys for navigating lists
    - (*) Add sort by feature
    - ( ) Dim menu options in the footer if they are not available to use
    - ( ) Overview can be selected on nc_select_year

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
        - (*) Add Transaction
        - (*) Edit
            - (*) Date
            - (*) Category
            - (*) Description
            - (*) Transaction Type
            - (*) Amount
        - (*) Confirm windows
        - (*) Categories
            - (*) When adding a transaction, select a category or add new
                    in an NCURSES menu.
        - ( ) Overview
            - (*) With bar graphs
            - ( ) Yearly summary
        - (*) Scroll through dates
        - ( ) Resizing
    - (*) Basic Bar Graph Trends

#### Bugs
    - (*) A line number is able to be selected (line 1) and it will edit the
            header line. Also line 1 is displayed as line 2. Probably something
            to do with the humanreadable target variable being subtracted by 2
    - (*) *n/a* Sorter places a previous year on line 3 when a more recent year is on
            line 2.
    - (*) Sorter edge case on adding a transaction to a month which does not
            match and is not greater than the target month. If the months in
            the CSV are all less than the target, the record will be 
            erroneously be added out of chronological order.
    - (*) Edge case where trying to access read via the TUI, fgets fails to
            read the line (because it doesn't exist).
    - (*) If editing a record's date causing no more records to exist for that
            year and month, the TUI doesn't refresh properly.
    - (*) *n/a* Sorter function can't handle adding a record between two years which
            doesn't already exist.
    - ( ) Input too short warning when entering a single digit line number in 
            CLI mode
    - (*) *n/a* Sorter still doesn't completely work. Adding months between months
            doesn't work as intended. If January has 2 records in it and a
            new record is added, like a record in May, the may record will be
            inserted between the two records in January.





#test
