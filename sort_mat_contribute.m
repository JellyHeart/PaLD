function C = sort_mat_contribute(D,beta)


% D is the distance matrix
if beta < 0
    error('beta must be positive');
end

if D' ~= D 
    error('distance matrix must be symmetric');
end

n = size(D,1);
C = zeros(n);


% get the sorted indices matrix 
sort_D_indices = get_sorted_indices(D);


% use sparse accumulator to get union of ux and uy
% need to sort each row of the distance matrix

for i = 1:(n-1)
    for j = (i+1):n
        
        % x and y are two points that we selected
        x = i;
        y = sort_D_indices(i,j);
        
        dx = D(x,:);% get the row of distance between x and all other points
        dy = D(y,:);% get the row of distance between y and all other points
        
        % ux contains the points that have a smaller distance with x
        % comparing with d(x,y)
        ux = sort_D_indices(i,1:j-1);
        
        
        % uy contains the points that have a smaller distance with y
        % comparing with d(x,y), to 
        % get uy we need find d(x,y) in the yth
        % row, uy_bound give the index where d(x,y) lives in the sorted
        % distance indices matrix
        uy_bound = find(sort_D_indices(y,:) == x);
        uy = sort_D_indices(y,1:uy_bound-1);
        
        % get the unique points in ux and uy
        b = zeros(1,n);
        
        b(1,ux) = 1;
        b(1,uy) = 1;
        uxy = find(b ~= 0);
        
      
    
        % need to add the equal parts
        u_size = size(uxy,2);
        if u_size ~= 0
            for  k = 1:u_size
                if find(sort_D_indices(uxy(k),x)) < find(sort_D_indices(uxy(k),y))
                    C(x,uxy(k)) = C(x,uxy(k)) + 1;
                else
                    C(y,uxy(k)) = C(y,uxy(k)) + 1; 
                end
            end
            C(x,uxy) = C(x,uxy)/(u_size);
            C(y,uxy) = C(y,uxy)/(u_size);
        end
            
      
    end
end

C = C/(n-1);



end