### Todo
    - [X] Break out user input into separate functions
    - Features: 
        - [X] Categories
            - [X] View transactions based on category
            - [X] Add categories
            - [X] Delete categories
            - [X] Allocate monthly budget by category
            - [X] List Categories
        - [X] Ability to edit a specific field
        - [X] Command line arguments
            - [X] Debug
            - [X] CLI Mode
    - [X] Get rid of strtok and use strsep
    - [ ] Create some unit tests for input validation
    - [X] Sorting The CSV By Date
    - [X] In the read function, implement a way to list what dates have data
            in them.
    - [X] In the read function, implement a better warning for accessing a 
            date range that doesn't contain any data.
    - [X] Display transaction type as a string instead of a bare number
    - [X] Show selected month and year at the top of the screen after
            selection on read
    - [X] Remove all reallocarray() functions for portability
    - [ ] After main feature set is done and working, refactor to make main.c
            have a clearer control flow purpose
    - [X] Implement visual truncation when there are not enough columns to
            display all the field data
    - [ ] Move to integer/fixed point arithmetic for financial calculations
    - [X] *n/a* Use realloc in sorter.c (sort_csv()) instead of having a fixed
            number of lines to index
    - [X] Add ability to escape the menu when accepting a user input
    - [X] Add page up/page down/end/home keys for navigating lists
    - [X] Add sort by feature
    - [X] Dim menu options in the footer if they are not available to use
    - [X] Make index_csv function in parser.c amortize memory allocation
    - [X] Create methods for managing the actual budgeting system of the
            program. This concerns budget.csv, defining budget category
            money allocation, file verification, user input, and an easy to
            use and view interface.
        - [X] Budget CSV parsing
        - [X] Sorting by Category should be the default behavior
        - [X] Show each category in the read loop and it's associated planned
                amount for each category and how much is remaining.
        - [X] Allow budget planning, I.E. making categories where no record
                currently exists.
        - [X] Handle deleting categories, possibly renaming as well. But
                probably not.
        - [ ] Orphaned categories handling
        - [X] Verify data between files and in the files themselves (The CSVs)
    - [X] Create data structures to handle budget planning by category
    - [X] Add confirmation prompts when editing a category line
    - [X] Add initialization function to handle opening files and getting
            vectors, deduplicating, and sorting for nc_select_year/month
    - [X] nc_edit_transaction function is ugly, it must be changed.
    - [ ] Have option to roll categories from previous month to the next month
    - [ ] Annotate tui_input.h
    - [ ] Menu key to (A)dd (F1) on main menu should allow the user to select
            creating a new transaction, creating a new category, roll over
            previous month's data to the target month, or exit.
    - [X] Add alerter when adding a category that already exists
    - [ ] Add a sidebar or somewhere to put info such as: money remaining,
            money left to still budget (zero dollar based budgeting), total
            income, etc.
    - [X] Create a color palette and assign different colors to
            different categories.
    - [ ] On the user input prompts, show relevant data on the window as the
            user goes from prompt to prompt. Such as, show the date entered so
            far and the remaining values left per category.
    - [X] Add field in BUDGET_DIR with transaction types. A category should be
            marked as an income or expense category.
        - [X] Use this new field to properly calculate "left to budget"
    - [ ] Remove list_records_by_year_old and list_records_by_month_old.
    - [ ] Refactor legacy_read_csv for CLI mode
    - [ ] Refactor any function that uses seek_n_fields with an integer literal
            to use a header parsing function to determine field location.
    - [X] Add csv header verification on program start.

#### Main Feature Set
    - [X] Create a transaction
    - [X] Create a category
    - [X] View     " "
    - [X] Update   " "
    - [X] Delete   " "
    - [X] ncurses <-- The big one
        - [X] Welcome Menu
        - [X] Read
            - [X] Add ability to scroll through records if they are too
                    big to fit on the screen
            - [X] Columns for fields for easier reading
        - [X] Add Transaction
        - [X] Edit
            - [X] Date
            - [X] Category
            - [X] Description
            - [X] Transaction Type
            - [X] Amount
        - [X] Confirm windows
        - [X] Categories
            - [X] When adding a transaction, select a category or add new
                    in an NCURSES menu.
        - [X] Overview
            - [X] With bar graphs
            - [X] Yearly summary
        - [X] Scroll through dates
        - [X] Resizing
    - [X] Basic Bar Graph Trends

#### Refactors
    - [X] Replace any function which dynamically allocates memory into a
            dynamically sized array with type Vec. This will prevent having
            to create the local realloc_counter that i've been using. It's
            not great.

#### Bugs
    - 1 [X] A line number is able to be selected (line 1) and it will edit the
            header line. Also line 1 is displayed as line 2. Probably something
            to do with the humanreadable target variable being subtracted by 2
    - 2 [X] *n/a* Sorter places a previous year on line 3 when a more recent year is on
            line 2.
    - 3 [X] Sorter edge case on adding a transaction to a month which does not
            match and is not greater than the target month. If the months in
            the CSV are all less than the target, the record will be 
            erroneously be added out of chronological order.
    - 4 [X] Edge case where trying to access read via the TUI, fgets fails to
            read the line (because it doesn't exist).
    - 5 [X] If editing a record's date causing no more records to exist for that
            year and month, the TUI doesn't refresh properly.
    - 6 [X] *n/a* Sorter function can't handle adding a record between two years which
            doesn't already exist.
    - 7 [ ] Input too short warning when entering a single digit line number in 
            CLI mode
    - 8 [X] *n/a* Sorter still doesn't completely work. Adding months between months
            doesn't work as intended. If January has 2 records in it and a
            new record is added, like a record in May, the may record will be
            inserted between the two records in January.
    - 9 [X] Budget sorter is jank, doesn't work.
    - 10 [X] refresh_on_detail_close_uniform in main.c when sorted by category doesn't
            re-color the categories, a new func needs to be made to handle this.
    - 11 [X] When selecting a category in ncurses, empty categories are not shown.
            The way the function retrieves categories needs to be changed.
    - 12 [X] Seg fault when deleting a category and that category is the only
            data to exist for a given month and year.
    - 13 [ ] There is nothing to handle the case where there are so many years
            with records that the terminal is not large enough to display
            them all. Years should horizontally scroll.
    - 14 [X] Cannot add a transaction if the user is inside of the nc_select_month
            loop.
    - 15 [X] Seg fault after selecting "READ" function from nc_main_menu in main.c
    - 16 [X] Several different integer type cmp's that need to be resolved.
    - 17 [X] On some systems the function that is clearing the footer on stdscr
                is being displayed as a question mark
    - 18 [X] After adding a transaction it requires two presses of the quit menu
                key to exit. Each subsequent transaction added requires +1 key
                presses.
    - 19 [ ] The overview window position is off, it looks weird. Not centered
                correctly.
    - 20 [X] When editing a transaction and creating a new category the category
                is not actually created but the record in data.csv is updated
                correctly.
    - 21 [ ] Sidebar does not refresh when transaction detail window opens.
    - 22 [ ] Left to budget value in sidebar is incorrect.
