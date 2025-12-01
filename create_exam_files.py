for i in range(1,22): 
    if i == 21: 
        with open(f"exams/9999.txt", "w") as f:
            f.write("9999")
    else: 
        if i in range(1,10): # 1-9
            with open(f"exams/000{i}.txt", "w") as f:   
                f.write(f"000{i}")
        elif i in range(10, 21): # 10-20
            with open(f"exams/00{i}.txt", "w") as f:   
                f.write(f"00{i}")
    