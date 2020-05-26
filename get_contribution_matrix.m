function [C,n] = get_contribution_matrix(D,beta)
% D is the distance matrix, the value of D(x,y) is the distance between x 
% and y (d(x,y)).
% beta is the parameter to control the radius of conflict focus, the output
% matrix C is the cohension matrix.

if beta < 0 
    error('beta must be positive');
end

if D' ~= D 
    error('distance matrix must be symmetric');
end


n = size(D,1);
C = zeros(n);

for x = 1:(n-1)
    for y = (x+1):n
        dx = D(x,:);
        dy = D(y,:);
        uxy = [find(dx <= beta*D(x,y))  find(dy <= beta*D(x,y))];
        uxy = unique(uxy);
        wx = sum(dx(uxy) < dy(uxy)) + 0.5*sum(dy(uxy) == dx(uxy));
        wy = sum(dy(uxy) < dx(uxy)) + 0.5*sum(dy(uxy) == dx(uxy));
        u_size = size(uxy,2);
        if u_size ~= 0
            C(x,uxy) = C(x,uxy) + wx/(u_size); 
            C(y,uxy) = C(y,uxy) + wy/(u_size);
        end
    end
end

C = C/(n-1);

end
        
  
